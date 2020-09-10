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

void init(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::LogToStderr();
}

int main(int argc, char** argv) {
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
    using path_state_t = plato::dense_state_t<std::vector<std::vector<int>>, partition_t>;
    using bitmap_spec_t        = plato::bitmap_t<>;

    auto out_degrees = plato::generate_dense_out_degrees_fg<uint32_t>(graph_info, *pdcsc, false);;

    // read subnode
    std::shared_ptr<bitmap_spec_t> existed(new bitmap_spec_t(graph_info.vertices_));
    existed->clear();
    {
        std::ifstream fin{FLAGS_subnode};
        int u;
        while (fin >> u) {
            existed->set_bit(u);
        }
    }

    // plato::bsp_opts_t opts;
    // opts.local_capacity_ = PAGESIZE;
    // opts.global_size_ = MBYTES;

    std::shared_ptr<path_state_t> curr_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
    std::shared_ptr<path_state_t> next_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
    std::shared_ptr<path_state_t> found_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));

    using context_spec_t = plato::mepa_ag_context_t<std::vector<std::vector<int>>>;
    using message_spec_t = plato::mepa_ag_message_t<std::vector<std::vector<int>>>;

    plato::vid_t actives = 0;
    for (uint32_t epoch_i = 0; epoch_i < FLAGS_n && actives < FLAGS_num; ++epoch_i) {

    if (epoch_i == 0) {
        plato::vid_t newers = next_path->foreach<int> (
            [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
                (*path).push_back(std::vector<int>(1, v_i));
                return 1;
            },
            existed.get()
        );
    } else {
      actives = plato::aggregate_message<std::vector<std::vector<int>>, int, graph_spec_t> (*pdcsc,
        [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
            std::vector<std::vector<int>> path_bin;
            for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
                plato::vid_t src = it->neighbour_;
                for (auto vpath: (*curr_path)[src]) {
                    bool is_conflict = ((vpath.size() == 1 && vpath[0] == v_i) ? true : false);
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
            if (path.size() >= std::max(FLAGS_k,(uint32_t)4) && path[0] == path[path.size()-1]) {
              (*found_path)[msg.v_i_].push_back(path);
            } else {
              (*next_path)[msg.v_i_].push_back(path);
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

  {  // save result
    plato::thread_local_fs_output os(FLAGS_output, (boost::format("%04d_") % cluster_info.partition_id_).str(), true);
    found_path->foreach<int> (
      [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
        if ((*path).size() == 0) return 0;
        auto& fs_output = os.local();
        fs_output << "node : " << v_i << ' ' << (*path).size() << "\n";
        return 0;
      }
    );
  }

  return 0;
}

