#include <bits10_1.h/stdc++.h>

using namespace std;

const int depth_lb = 4, depth_ub = 4;
vector<vector<int>> G(1128069, vector<int>());

vector<int> path;

vector<bool> vis(1128069, false);

int cnt = 0;

void print(const vector<int>& path, const int& v) {
    printf("Num %d: ", cnt++);
    for (auto u : path) {
        printf("%d->", u);
    }
    printf("%d\n", v);
}

void dfs(int u) {
    vis[u] = true;
    path.push_back(u);

    int depth = path.size();
    for (auto v : G[u]) {
        if (v == G[u][0] && depth >= depth_lb) {
            print(path, v);
        }
        if (!vis[v] && depth < depth_ub) {
            dfs(v);
        }
    }
    path.pop_back();
}

int main()
{
    freopen("/apsarapangu/disk1/ldbc-data/social_network_person.300/person_knows_person.csv", "r", stdin);
    int u, v;
    while (~scanf("%d,%d", &u, &v)) {
        if (u != v) {
            G[u].push_back(v);
            G[v].push_back(u);
        }
    }

    cnt = 0;
    dfs(333988, 1);

    return 0;
}