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
    // int v[4] = {3800,10244,3235,4173};
    int v[200] = {588045,946274,839998,466040,218545,1091459,1081557,100593,307442,198353,885292,137239,853634,503711,301856,1107688,1098067,181729,1037791,902153,5418,725807,265710,962189,754930,629280,197559,1058248,359381,777879,411751,364603,1108219,856242,521578,123915,60958,803222,910363,157243,699486,812705,507602,314558,424372,1126631,105063,476609,955278,494975,280544,645669,488497,618029,1061451,517442,919166,141439,842582,340248,874534,1088688,535859,152400,1041081,533945,31331,675997,63646,566533,1089984,37542,307337,521376,62395,1127690,1021955,651097,987883,889814,82385,437866,956827,608010,1026426,750246,1082423,168043,1115329,156840,404606,844509,149085,99248,836990,476030,295691,718895,403153,916459,816001,727944,612548,606511,1097616,118565,961453,681106,812121,851704,607181,485574,91534,381086,446567,979701,98147,1088998,764127,270723,1028357,149140,235134,317476,271125,229481,620336,434047,862195,443493,336915,521077,491965,1110239,3871,484714,136840,1014690,520717,825216,880359,284529,535014,1127453,585691,662065,854373,624474,487724,855674,457228,227673,713156,1039499,850675,359998,44204,1058776,120156,946350,823533,145657,527199,292346,50868,868383,948024,138384,104628,336197,1013038,218223,581869,894490,509500,661613,431385,268325,651029,763547,303659,11088,794731,368902,880924,1051555,816081,781435,1114400,471567,877225,953953,1044096,746032,95457,696093,373305,856955,96909,38743};
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