#include <cstdint>
#include <cstdlib>
#include <utility>

#include "plato/util/perf.hpp"
#include "plato/util/atomic.hpp"
#include "plato/graph/graph.hpp"

DEFINE_string(input,       "",     "input file, in csv format, without edge data");
DEFINE_string(output,      "",      "output directory");
DEFINE_bool(is_directed,   true,  "is graph directed or not");
DEFINE_int32(alpha,        -1,     "alpha value used in sequence balance partition");
DEFINE_bool(part_by_in,    false,  "partition by in-degree");
DEFINE_uint32(n,           5,      "at most N-hop");
DEFINE_uint32(k,           4,      "len of cycle >= k");
DEFINE_uint32(num,         1,      "num of cycle");
DEFINE_string(subnode,     "",     "subnode");
DEFINE_uint32(sub_round,   10,     "nodes number for one round");
DEFINE_string(to_run,      "",     "round to run");


void init(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
}

unordered_set<int> to_run_;

void set_run_info() {
  std::ifstream fin{FLAGS_to_run};
  if (!fin) return;
  
  int u;
  while (fin >> u) {
    to_run_.insert(u);
  }
}

int main(int argc, char** argv) {
  double total_time = 0;
  double mx_time = -1;
  double min_time = 1e8;

  plato::stop_watch_t watch;
  auto& cluster_info = plato::cluster_info_t::get_instance();

  init(argc, argv);
  cluster_info.initialize(&argc, &argv);

  // init graph
  plato::graph_info_t graph_info(FLAGS_is_directed);
  auto pdcsc = plato::create_dcsc_seqs_from_path<plato::empty_t>(&graph_info, FLAGS_input,
      plato::edge_format_t::CSV, plato::dummy_decoder<plato::empty_t>,
      FLAGS_alpha, FLAGS_part_by_in);

  using graph_spec_t         = std::remove_reference<decltype(*pdcsc)>::type;
  using partition_t          = graph_spec_t::partition_t;
  using adj_unit_list_spec_t = graph_spec_t::adj_unit_list_spec_t;
  using path_state_t         = plato::dense_state_t<std::vector<std::vector<int>>, partition_t>;
  using bitmap_spec_t        = plato::bitmap_t<>;
  using context_spec_t       = plato::mepa_ag_context_t<std::vector<std::vector<int>>>;
  using message_spec_t       = plato::mepa_ag_message_t<std::vector<std::vector<int>>>;

  plato::bsp_opts_t opts;
  opts.local_capacity_ = PAGESIZE/16;
  opts.global_size_ = 16*MBYTES;
  
  std::vector<std::mutex> mtx_next(graph_info.vertices_), mtx_found(graph_info.vertices_);
  
  set_run_info();

  std::ifstream fin{FLAGS_subnode};
  for (int round_i = 1; round_i <= 10; ++round_i) {
    std::shared_ptr<path_state_t> curr_path(new path_state_t(graph_info.vertices_, pdcsc->partitioner()));
    std::shared_ptr<path_state_t> next_path(new path_state_t(graph_info.vertices_, pdcsc->partitioner()));
    std::shared_ptr<path_state_t> found_path(new path_state_t(graph_info.vertices_, pdcsc->partitioner()));
    std::shared_ptr<bitmap_spec_t> existed(new bitmap_spec_t(graph_info.vertices_));
    // read subnode
    existed->clear();
    {
      int u;
      for (int i = 0; i < std::min((uint32_t)20, FLAGS_sub_round); ++i) {
        fin >> u;
        existed->set_bit(u);
	      printf("Round %d: find %d\n", round_i, u);
      }
      for (int i = FLAGS_sub_round; i < 20; ++i) {
        fin >> u;
      }
    }

    if (to_run_.find(round_i) == to_run_.end()) {
      continue;
    }
    
    watch.mark("t_oneround");
    watch.mark("t_algo");

    plato::vid_t actives = 0;
    for (uint32_t epoch_i = 0; epoch_i < FLAGS_n && actives < FLAGS_num; ++epoch_i) {
      if (0 == cluster_info.partition_id_) {
        LOG(INFO) << "[epoch-" << epoch_i  << "] init-next cost: "
          << watch.show("t_oneround") / 1000.0 << "s" << "\nactives :" << actives;
      }
      watch.mark("t_oneround");

      if (epoch_i == 0) {
        next_path->foreach<int> (
          [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
            (*path).push_back(std::vector<int>(1, v_i));
            return 1;
          },
          existed.get()
        );
      } else {
        plato::aggregate_message<std::vector<std::vector<int>>, int, graph_spec_t> (*pdcsc,
          [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
            std::vector<std::vector<int>> path_bin;
            for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
              plato::vid_t src = it->neighbour_;
              for (auto vpath: (*curr_path)[src]) {
                bool is_conflict = (vpath[0] == v_i && (vpath.size()+1 < std::max(FLAGS_k,(uint32_t)4)) ? true : false);
                for (int i = 1; i < vpath.size(); i++) {
                  if (vpath[i] == v_i) {
                    is_conflict = true;
                    break;
                  }
                }

                if (!is_conflict) {
                  vpath.push_back(v_i);
                  path_bin.push_back(vpath);
                }
              }
            }
            context.send(message_spec_t { v_i, path_bin });
          },
          [&](int /*p_i*/, message_spec_t& msg) {
            for (auto path : msg.message_) {
              if (path[0] == path[path.size()-1]) {
                mtx_found[msg.v_i_].lock();
                (*found_path)[msg.v_i_].push_back(path);
                mtx_found[msg.v_i_].unlock();
              } else {
                mtx_next[msg.v_i_].lock();
                (*next_path)[msg.v_i_].push_back(path);
                mtx_next[msg.v_i_].unlock();
              }
            }
            return 1;
          }
          ,opts
        );
      }

      actives = found_path->foreach<int> (
        [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
          return (*path).size();
        }
      );

      std::swap(curr_path, next_path);
      
      next_path->foreach<int> (
        [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
          path->clear();
          return 0;
        }
      );
    }

    if (0 == cluster_info.partition_id_) {
      double time_cost = watch.show("t_algo") / 1000.0;
      total_time += time_cost;
      min_time = std::min(time_cost, min_time);
      mx_time = std::max(time_cost, mx_time);
      LOG(INFO) << "n-hop cost: " << time_cost << "s";
    }

    watch.mark("t_output");
    {
      // plato::thread_local_fs_output os(FLAGS_output, (boost::format("%04d_") % cluster_info.partition_id_).str(), true);
      found_path->foreach<int> (
        [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
          if ((*path).size() == 0) return 0;
          
          printf("Node: %d - %d\n", v_i, (*path).size());
          // auto& fs_output = os.local();
          // fs_output << "node : " << v_i << "\n";
          // for (auto perpath : (*path)) {
          //   std::string log;
          //   for (auto vnode : perpath) {
          //     log += std::to_string(v) + "->";
          //   }
          //   fs_output << msg.v_i_ << " : " << << log << "\n";
          // }
          return 0;
        }
      );
    }
  }
  if (0 == cluster_info.partition_id_) {
    printf("max_time: %.3lf, min_time: %.3lf, avg_time: %.3lf\n", mx_time, min_time, total_time/10.0);
  }
  return 0;
}

