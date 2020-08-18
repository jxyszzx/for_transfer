/*
  Tencent is pleased to support the open source community by making
  Plato available.
  Copyright (C) 2019 THL A29 Limited, a Tencent company.
  All rights reserved.

  Licensed under the BSD 3-Clause License (the "License"); you may
  not use this file except in compliance with the License. You may
  obtain a copy of the License at

  https://opensource.org/licenses/BSD-3-Clause

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
  implied. See the License for the specific language governing
  permissions and limitations under the License.

  See the AUTHORS file for names of contributors.
*/

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
DEFINE_uint32(k,           3,      "len of cycle >= k");
DEFINE_uint32(num,         1,      "num of cycle");
DEFINE_string(subnode,     "",     "subnode");

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
  using path_state_t = plato::dense_state_t<std::vector<std::vector<int>>, partition_t>;
  using bitmap_spec_t        = plato::bitmap_t<>;

  watch.mark("t_odegree");
  auto out_degrees = plato::generate_dense_out_degrees_fg<uint32_t>(graph_info, *pdcsc, false);;
  
  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "generate out-degrees from graph cost: " << watch.show("t_odegree") / 1000.0 << "s";
  }

  // read subnode
  // bitmap_spec_t existed(graph_info.vertices_);
  std::shared_ptr<bitmap_spec_t> existed(new bitmap_spec_t(graph_info.vertices_ + 1));
  existed->clear();
  {
    std::ifstream fin{FLAGS_subnode};
    // std::unordered_set subnode;
    int u;
    while (fin >> u) {
      existed->set_bit(u);
      // subnode.insert(u);
    }
  }

plato::bsp_opts_t opts;
opts.local_capacity_ = PAGESIZE;
opts.global_size_ = MBYTES;

  std::shared_ptr<path_state_t> curr_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
  std::shared_ptr<path_state_t> next_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
  std::shared_ptr<path_state_t> found_path(new path_state_t(graph_info.max_v_i_, pdcsc->partitioner()));

  watch.mark("t_oneround");
  watch.mark("t_algo");

  using context_spec_t = plato::mepa_ag_context_t<std::vector<std::vector<int>>>;
  using message_spec_t = plato::mepa_ag_message_t<std::vector<std::vector<int>>>;

  std::mutex vector_mutex;
  std::mutex found_mutex;

  plato::vid_t actives = 0;
  for (uint32_t epoch_i = 0; epoch_i < FLAGS_n && actives < FLAGS_num; ++epoch_i) {
    if (0 == cluster_info.partition_id_) {
      LOG(INFO) << "[epoch-" << epoch_i  << "] init-next cost: "
        << watch.show("t_oneround") / 1000.0 << "s" << "\nactives :" << actives;
    }
    watch.mark("t_oneround");

    if (epoch_i == 0) {
      plato::vid_t newers = next_path->foreach<int> (
      [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
        // LOG(INFO) << "node : " << v_i;
        // for (auto perpath : (*path)) {
        //   std::string log; 
        //   for (auto vnode : perpath) {
        //     log += std::to_string(vnode) + ",";
        //   }
        //   LOG(INFO) << log;
        // }
        (*path).push_back(std::vector<int>(1, v_i));
        return 1;
      },
      existed.get()
    );
      // actives = plato::aggregate_message<std::vector<std::vector<int>>, int, graph_spec_t> (*pdcsc,
      //   [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
      //     if (!existed.get_bit(v_i)) { return; }

      //     std::vector<std::vector<int>> path_bin;
      //     path_bin.push_back(std::vector<int>(1, v_i));
      //     context.send(message_spec_t { v_i, path_bin });
      //   },
      //   [&](int /*p_i*/, message_spec_t& msg) {
      //     vector_mutex.lock();
      //     for (auto path : msg.message_) {
      //       (*next_path)[msg.v_i_].push_back(path);
      //     }
      //     vector_mutex.unlock();
      //     return 1;
      //   }
      // );
    } else {
      actives = plato::aggregate_message<std::vector<std::vector<int>>, int, graph_spec_t> (*pdcsc,
        [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
          // if (!existed.get_bit(v_i)) { return; }

          std::vector<std::vector<int>> path_bin;
          for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
            plato::vid_t src = it->neighbour_;
            // if (!existed.get_bit(src)) { continue; }
// LOG(INFO) << src << " is nbr of " << v_i;
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
// std::string log;
// for (auto v : path) {
//   log += std::to_string(v) + "->";
// }
// LOG(INFO) << msg.v_i_ << " : " << log;

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


    // curr_path->foreach<int> (
    //   [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
    //     LOG(INFO) << "node : " << v_i;
    //     for (auto perpath : (*path)) {
    //       std::string log; 
    //       for (auto vnode : perpath) {
    //         log += std::to_string(vnode) + ",";
    //       }
    //       LOG(INFO) << log;
    //     }
    //     return 0;
    //   }
    // );

    // LOG(INFO) << "FOUND";

    actives = found_path->foreach<int> (
      [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
        // LOG(INFO) << "node : " << v_i;
        // for (auto perpath : (*path)) {
        //   std::string log; 
        //   for (auto vnode : perpath) {
        //     log += std::to_string(vnode) + ",";
        //   }
        //   LOG(INFO) << log;
        // }
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
    LOG(INFO) << "n-hop cost: " << watch.show("t_algo") / 1000.0 << "s";
  }

  watch.mark("t_output");
  {  // save result to hdfs
    plato::thread_local_fs_output os(FLAGS_output, (boost::format("%04d_") % cluster_info.partition_id_).str(), true);
    found_path->foreach<int> (
      [&](plato::vid_t v_i, std::vector<std::vector<int>>* path) {
        if ((*path).size() == 0) return 0;
        auto& fs_output = os.local();

        fs_output << "node : " << v_i << ' ' << (*path).size() << "\n";
        // fs_output << "node : " << v_i << "\n";
        // for (auto perpath : (*path)) {
        //   for (auto vnode : perpath) {
        //     fs_output << vnode << ",";
        //   }
        //   fs_output << "\n";
        // }
        return 0;
      }
    );
  }
  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "save result cost: " << watch.show("t_output") / 1000.0 << "s";
  }

  return 0;
}

