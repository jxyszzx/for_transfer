#include <bits/stdc++.h>

using namespace std;

vector<pair<int, int>> edges_, new_edges_;
vector<int> nodes_;

set<int> s;
map<int, int> old_to_new_;

void map_node(int u) {
    if (s.find(u) == s.end()) {
        old_to_new_[u] = s.size();
        s.insert(u);
    }
}

int main(int argc, char ** argv)
{
    // input parameter
    if (argc != 3) {
        puts("Usage: [program] [max_num of person_know_person part] [max_num of person part]");
        exit(0);
    }
    int pkp_range = atoi(argv[1]);
    int p_range = atoi(argv[2]);

    // file location
    string pre_pkp = "/apsarapangu/disk1/ldbc-data/social_network_person.300/person_knows_person/part-";
    string pre_p = "/apsarapangu/disk1/ldbc-data/social_network_person.300/person/part-";

    string output_pkp = "/apsarapangu/disk1/ldbc-data/social_network_person.300/person_knows_person.csv";
    string output_p = "/apsarapangu/disk1/ldbc-data/social_network_person.300/person.txt";
    string output_map = "./node_map.txt";

    // deal with pkp
    for (int i = 0; i <= pkp_range; ++i) {
        char name_id[10];
        sprintf(name_id, "%05d", i);
        string name_pkp = pre_pkp + string(name_id);
        FILE* fin = freopen(name_pkp.c_str(), "r", stdin);

        int u, v, tmp_int;
        char tmp_chars[50];
        while (~scanf("%d|%d|%d|%s\n", &u, &tmp_int, &v, tmp_chars)) {
            edges_.push_back({u, v});
            map_node(u);
        }
    }
    for (auto edge : edges_) {
        map_node(edge.second);
    }

    // deal with pkp
    for (int i = 0; i <= p_range; ++i) {
        char name_id[10];
        sprintf(name_id, "%05d", i);
        string name_p = pre_p + string(name_id);
        FILE* fin = fopen(name_p.c_str(), "r");

        int u;
        char tmp_chars[1000];
        while (fgets(tmp_chars, 900, fin)) {
            char *u_str = strtok(tmp_chars, "|");
            u = atoi(u_str);

            nodes_.push_back(u);
            map_node(u);
        }
        // rewind(fin);
    }

    for(auto edge : edges_) {
        new_edges_.push_back({old_to_new_[edge.first], old_to_new_[edge.second]});
    }
    // sort(new_edges_.begin(), new_edges_.end());
    swap(new_edges_, edges_);

    // make up to N-1
    int max_id = s.size()-1;
    cout << "max_id : " << max_id << endl;
    // cout << "start : " << edges_[edges_.size()-1].first << endl;
    for (int i = edges_[edges_.size()-1].first+1; i <= max_id; ++i) {
        edges_.push_back({i, i});
    }

    FILE *fout = freopen(output_pkp.c_str(), "w", stdout);
    // for (auto edge : edges_) {
    //     printf("%d,%d\n", old_to_new_[edge.first], old_to_new_[edge.second]);
    // }
    for (auto edge : edges_) {
        printf("%d,%d\n", edge.first, edge.second);
    }
    fout = freopen(output_p.c_str(), "w", stdout);
    for (auto node : nodes_) {
        printf("%d\n", old_to_new_[node]);
    }
    fout = freopen(output_map.c_str(), "w", stdout);
    int v[4] = {3800,10244,3235,4173};
    for (auto pair : old_to_new_) {
        for (int i = 0; i < 4; ++i) {
            if (v[i] == pair.first) {
                printf("%d %d\n", pair.first, pair.second);
                break;
            }
        }
    }

    // freopen("CON", "w", stdout);///dev/console
    return 0;
}