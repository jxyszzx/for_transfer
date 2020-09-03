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

#define PRINT_DEBUG

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

// void print(std::shared_ptr<matching_state_t> x, string from, std::shared_ptr<bitmap_spec_t> active) {
//   s->foreach<int> (
//     [&](plato::vid_t v_i, int* val) {
//       LOG(INFO) << from << "[" << v_i << "]: " << (*val);
//       return 0;
//     }, active.get()
//   );
// }

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
  using matching_state_t = plato::dense_state_t<int, partition_t>;
  using bitmap_spec_t        = plato::bitmap_t<>;

  auto print = [&](std::shared_ptr<matching_state_t> x, std::string from, std::shared_ptr<bitmap_spec_t> active) {
    x->foreach<int> (
      [&](plato::vid_t v_i, int* val) {
        LOG(INFO) << from << "[" << v_i << "]: " << (*val);
        return 0;
      }, active.get()
    );
  };
  
// plato::bsp_opts_t opts;
// opts.local_capacity_ = PAGESIZE;
// opts.global_size_ = MBYTES;
  
  std::shared_ptr<matching_state_t> p(new matching_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
  std::shared_ptr<matching_state_t> s(new matching_state_t(graph_info.max_v_i_, pdcsc->partitioner()));
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
  s->foreach<int> (
    [&](plato::vid_t v_i, int* val) {
      *val = -1;
      return 1;
    }
  );
  plato::vid_t actives = p->foreach<int> (
    [&](plato::vid_t v_i, int* val) {
      *val = -1;
      return 1;
    }
  );
  
  for (uint32_t epoch_i = 0; actives != 0; ++epoch_i) {
    if (0 == cluster_info.partition_id_) {
      LOG(INFO) << "[epoch-" << epoch_i  << "] init-next cost: "
        << watch.show("t_oneround") / 1000.0 << "s" << "\nactives :" << actives;
    }
    watch.mark("t_oneround");

    p->foreach<int> (
      [&](plato::vid_t v_i, int* val) {
        *val = -1;
        return 1;
      }, curr.get()
    );

    actives = plato::aggregate_message<int, int, graph_spec_t> (*pdcsc,
      [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!curr->get_bit(v_i)) { return; }

        int mx = -1;
        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
// LOG(INFO) << src << " is nbr of " << v_i;
          if ((*s)[src] == -1) {
            if (mx == -1) {
              mx = src;
            } else {
              mx = std::max(mx, (int)src);
            }
          }
        }
        if (mx != -1) {
          context.send(message_spec_t { v_i, mx });
        }
      },
      [&](int /*p_i*/, message_spec_t& msg) {
        plato::write_max(&((*p)[msg.v_i_]), msg.message_);
        next->set_bit(msg.v_i_);
        return 1;
      }
      // ,opts
    );

#ifdef PRINT_DEBUG
LOG(INFO) << "phase 1";
print(s, "s", all);
print(p, "p", all);
#endif

    // step 2
    actives = plato::aggregate_message<std::vector<int>, int, graph_spec_t> (*pdcsc,
      [&](const context_spec_t2& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!curr->get_bit(v_i) || !next->get_bit(v_i)) { return; }

        std::vector<int> _candidate;
        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
LOG(INFO) << src << " is nbr of " << v_i << ", p:" << (*p)[src];
          if ((*p)[src] == v_i) {
            _candidate.push_back(src);
          }
        }
        if (!_candidate.empty()) {
          context.send(message_spec_t2 { v_i, _candidate });
        }
      },
      [&](int /*p_i*/, message_spec_t2& msg) {
        for (auto v : msg.message_) {
LOG(INFO) << "Receive: " << msg.v_i_ << '-' << v;
          if ((*p)[msg.v_i_] == v) {
            next_p2->set_bit(msg.v_i_);
            (*s)[msg.v_i_] = (*p)[msg.v_i_];
LOG(INFO) << "Receive: " << v << ' ' << msg.v_i_;
            return 1;
          }
        }
        return 0;
      }
      // ,opts
    );
#ifdef PRINT_DEBUG
LOG(INFO) << "phase 2";
print(s, "s", all);
print(p, "p", all);

LOG(INFO) << next_p2->count();
#endif

    // step 3
next_p2->sync();
LOG(INFO) << next_p2->count();

    plato::aggregate_message<int, int, graph_spec_t> (*pdcsc,
      [&](const context_spec_t& context, plato::vid_t v_i, const adj_unit_list_spec_t& adjs) {
        if (!next_p2->get_bit(v_i)) { return; }

        for (auto it = adjs.begin_; adjs.end_ != it; ++it) {
          plato::vid_t src = it->neighbour_;
LOG(INFO) << "phase 3 " << src << " is nbr of " << v_i;
          if ((*p)[src] == v_i && (*s)[src] == -1) {
            next_p3->set_bit(src);
          }
        }
      },
      [&](int /*p_i*/, message_spec_t& msg) {
        return 0;
      }
      // ,opts
    );

    std::swap(next_p3, curr);
    next_p3->clear();
    next_p2->clear();
    next->clear();

    curr->sync();
    actives = curr->count();
  } 

  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "max-matching-fast cost: " << watch.show("t_algo") / 1000.0 << "s";
  }

  actives = s->foreach<int> (
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
    s->foreach<int> (
      [&](plato::vid_t v_i, int* val) {
        if ((*val) != -1) {
          auto& fs_output = os.local();
          fs_output << "s[" << v_i << "]:" << (*val) << "\n"; 
        }  
        return 0;
      }
    );
  }

  if (0 == cluster_info.partition_id_) {
    LOG(INFO) << "save result cost: " << watch.show("t_output") / 1000.0 << "s";
  }

  return 0;
}

