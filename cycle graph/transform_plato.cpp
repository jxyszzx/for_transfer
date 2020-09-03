#include <bits/stdc++.h>

using namespace std;

vector<pair<int, int>> edges_;

unordered_map<int, int> old_to_new_;

void map_node(int u) {
    if (old_to_new_.find(u) == old_to_new_.end()) {
        old_to_new_[u] = old_to_new_.size();
    }
}

int main(int argc, char ** argv)
{
    // input parameter
    if (argc != 2) {
        puts("Usage: [program] [graph_name]");
        exit(0);
    }

    string name = string(argv[1]);
    string output_pkp = "/apsarapangu/disk1/longbin/datasets/"+name+"/graph_plato.csv";
    
    // deal with pkp
    {
        int cnt = 0;
        string name_pkp = "/apsarapangu/disk1/longbin/datasets/"+name+"/graph.txt";
        FILE* fin = freopen(name_pkp.c_str(), "r", stdin);

        int u, v;
        while (~scanf("%d%d", &u, &v)) {
            if (u == v) continue;
            edges_.push_back({u, v});
            map_node(u);

            if (++cnt % 2000000 == 0) printf("Deal %d\n", cnt);
        }
    }
    for (auto edge : edges_) {
        map_node(edge.second);
    }

    for(auto &edge : edges_) {
        edge = {old_to_new_[edge.first], old_to_new_[edge.second]};
    }

    // MAX_ID
    int max_id = s.size()-1;
    cout << "max_id : " << max_id << endl;

    // OUTPUT   
    FILE *fout = freopen(output_pkp.c_str(), "w", stdout);
    for (auto edge : edges_) {
        printf("%d,%d\n", edge.first, edge.second);
    }

    return 0;
}