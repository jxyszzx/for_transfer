#include <bits/stdc++.h>

using namespace std;

int main()
{
    int x = 0;

    freopen("graph.txt", "w", stdout);

    printf("0 1\n");

    for (int cur = 0; cur < 1000; ++cur) {
        int i = cur*2;
        printf("%d %d\n", i, i+2);
        printf("%d %d\n", i+1, i+2);
        printf("%d %d\n", i+1, i+3);
        printf("%d %d\n", i+2, i+3);
    }

    return 0;
}