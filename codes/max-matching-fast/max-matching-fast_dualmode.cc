#include <cstdint>
#include <cstdlib>
#include <utility>

#include "plato/util/perf.hpp"
#include "plato/util/atomic.hpp"
#include "plato/graph/graph.hpp"

#define PHASE_TIME

DEFINE_string(input,       "",     "input file, in csv format, without edge data");
DEFINE_string(output,      "",      "output directory");
DEFINE_bool(is_directed,   true,  "is graph directed or not");
DEFINE_int32(alpha,        -1,     "alpha value used in sequence balance partition");
DEFINE_bool(part_by_in,    false,  "partition by in-degree");

bool string_not_empty(const char*, const std::string& value) {
  if (0 == value.length()) { return false; }
  return true;
}

void init(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
}

int main(int argc, char** argv) {
  using bcsr_spec_t          = plato::bcsr_t<plato::empty_t, plato::sequence_balanced_by_destination_t>;
  using dcsc_spec_t          = plato::dcsc_t<plato::empty_t, plato::sequence_balanced_by_source_t>;
  using partition_bcsr_t     = bcsr_spec_t::partition_t;
  using matching_state_t     = plato::dense_state_t<int, partition_bcsr_t>;
  using bitmap_spec_t        = plato::bitmap_t<>;
  using adj_unit_list_spec_t = bcsr_spec_t::adj_unit_list_spec_t;

  plato::stop_watch_t watch;
  auto& cluster_info = plato::cluster_info_t::get_instance();

  init(argc, argv);
  cluster_info.initialize(&argc, &argv);

  // init graph
  plato::graph_info_t graph_info(FLAGS_is_directed);
  auto graph = plato::create_dualmode_seq_from_path<plato::empty_t>(&graph_info, FLAGS_input,
      plato::edge_format_t::CSV, plato::dummy_decoder<plato::empty_t>,
      FLAGS_alpha, FLAGS_part_by_in);

  auto partition_view = graph.first.partitioner()->self_v_view();
  
  matching_state_t p(graph_info.max_v_i_, graph.first.partitioner());
  matching_state_t s(graph_info.max_v_i_, graph.first.partitioner());
  // bitmap_spec_t existed(graph_info.vertices_);
  std::shared_ptr<bitmap_spec_t> curr(new bitmap_spec_t(graph_info.vertices_));
  std::shared_ptr<bitmap_spec_t> next(new bitmap_spec_t(graph_info.vertices_));
  std::shared_ptr<bitmap_spec_t> next_p2(new bitmap_spec_t(graph_info.vertices_));
  std::shared_ptr<bitmap_spec_t> next_p3(new bitmap_spec_t(graph_info.vertices_));
  
  curr->fill();
  next->clear();
  next_p2->clear();
  next_p3->clear();

  //
  std::shared_ptr<bitmap_spec_t> all(new bitmap_spec_t(graph_info.vertices_));
  all->fill();
  //

  watch.mark("t_oneround");
  watch.mark("t_algo");

  using context_spec_t = plato::mepa_ag_context_t<int>;
  using message_spec_t = plato::mepa_ag_message_t<int>;
  using context_spec_t2 = plato::mepa_ag_context_t<std::vector<int>>;
  using message_spec_t2 = plato::mepa_ag_message_t<std::vector<int>>;

  // init  
  s.fill(-1);
  p.fill(-1);
  plato::vid_t actives = graph_info.vertices_;

  for (uint32_t epoch_i = 0; actives != 0; ++epoch_i) {
    if (0 == cluster_info.partition_id_) {
      printf("[epoch-%d] init-next cost: %.3f(s), actives : %d\n", epoch_i, watch.show("t_oneround") / 1000.0, actives);
    }
    watch.mark("t_oneround");

#ifdef PHASE_TIME
watch.mark("phase_1");
#endif

    p.foreach<int> (
      [&](plato::vid_t v_i, int* val) {
        *val = -1;
        return 1;
      }, curr.get()
    );

    plato::aggregate_message<int, int, dcsc_spec_t> (graph.second,
      [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!curr->get_bit(v_i)) { return; }

        int mx = -1;
        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
          if ((s)[src] == -1) {
            mx = std::max(mx, (int)src);
          }
        }
        if (mx != -1) {
          context.send(message_spec_t { v_i, mx });
        }
      },
      [&](int /*p_i*/, message_spec_t& msg) {
        plato::write_max(&((p)[msg.v_i_]), msg.message_);
        next->set_bit(msg.v_i_);
        return 0;
      }
      // ,opts
    );
    next->sync();

// printf("Phase 1: %d\n", next->count());

#ifdef PHASE_TIME
printf("Phase 1: cost %.3f(s)\n", watch.show("phase_1") / 1000.0);
#endif


    // step 2
#ifdef PHASE_TIME
watch.mark("phase_2");
#endif
    plato::aggregate_message<std::vector<int>, int, dcsc_spec_t> (graph.second,
      [&](const context_spec_t2& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!curr->get_bit(v_i) || !next->get_bit(v_i)) { return; }

        std::vector<int> _candidate;
        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
          if ((p)[src] == v_i) {
            _candidate.push_back(src);
          }
        }
        if (!_candidate.empty()) {  // ------ different communications
          context.send(message_spec_t2 { v_i, _candidate });
        }
      },
      [&](int /*p_i*/, message_spec_t2& msg) {
        if ((s)[msg.v_i_] == -1) {  // ------ if useful, one more traverse
          for (auto v : msg.message_) {
            if ((p)[msg.v_i_] == v) {
              next_p2->set_bit(msg.v_i_);
              (s)[msg.v_i_] = (p)[msg.v_i_];
              return 1;
            }
          }
        }
        return 0;
      }
      // ,opts
    );
    next_p2->sync();

#ifdef PHASE_TIME
printf("Phase 2: cost %.3f(s)\n", watch.show("phase_2") / 1000.0);
#endif


    auto active_view = plato::create_active_v_view(partition_view, *next_p2);

    struct broadcast_msg_t {
      plato::vid_t v_i_;
      plato::vid_t message_;
    };
    using broadcast_ctx_t = plato::mepa_bc_context_t<broadcast_msg_t>;

    plato::broadcast_message<broadcast_msg_t, plato::vid_t> (active_view,
        [&](const broadcast_ctx_t& context, plato::vid_t v_i) {
          context.send(broadcast_msg_t { v_i, (s)[v_i] } );
        },
        [&](int /* p_i */, broadcast_msg_t& msg) {
          next_p2->set_bit(msg.message_);
          (s)[msg.message_] = (p)[msg.message_];
          // auto neighbours = graph.first.neighbours(msg.v_i_);
          // for (auto it = neighbours.begin_; neighbours.end_ != it; ++it) {
          //   plato::vid_t dst = it->neighbour_;
          //   if ( dst == msg.message_ ) {
          //     next_p2->set_bit(dst);
          //     (s)[dst] = (p)[dst];
          //     return 1;
          //   }
          // }
          return 0;
        }
      );
    next_p2->sync();

printf("Phase 2: %d\n", next_p2->count());
    // step 3
#ifdef PHASE_TIME
watch.mark("phase_3");
#endif

    plato::aggregate_message<int, int, dcsc_spec_t> (graph.second,
      [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!next_p2->get_bit(v_i)) { return; }

        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
          if ((p)[src] == v_i && (s)[src] == -1) {
            next_p3->set_bit(src);
          }
        }
      },
      [&](int /*p_i*/, message_spec_t& msg) {
        return 0;
      }
      // ,opts
    );
    next_p3->sync();
    actives = next_p3->count();

#ifdef PHASE_TIME
printf("Phase 3: cost %.3f(s)\n", watch.show("phase_3") / 1000.0);
#endif
    std::swap(next_p3, curr);
    next_p3->clear();
    next_p2->clear();
    next->clear();
  } 

  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "max-matching-fast cost: " << watch.show("t_algo") / 1000.0 << "s";
  }

  actives = s.foreach<int> (
    [&](plato::vid_t v_i, int* val) {
      if ((*val) != -1) {
        return 1;
      }
      return 0;
    }
  );

  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "number of matching pairs=" << actives;
  }

  watch.mark("t_output");
  {  // save result to hdfs
    plato::thread_local_fs_output os(FLAGS_output, (boost::format("%04d_") % cluster_info.partition_id_).str(), true);
    s.foreach<int> (
      [&](plato::vid_t v_i, int* val) {
        // if ((*val) != -1) {
          auto& fs_output = os.local();
          fs_output << "s[" << v_i << "]:" << (*val) << "\n"; 
        // }  
        return 0;
      }
    );

    p.foreach<int> (
      [&](plato::vid_t v_i, int* val) {
          auto& fs_output = os.local();
          fs_output << "p[" << v_i << "]:" << (*val) << "\n"; 
        return 0;
      }
    );
  }

  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "save result cost: " << watch.show("t_output") / 1000.0 << "s";
  }

  return 0;
}

