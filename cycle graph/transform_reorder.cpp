#include <bits/stdc++.h>

using namespace std;

int main()
{
    freopen("node_map.txt", "r", stdin);
    freopen("node_map_reorder.txt", "w", stdout);

    int u, v;
    map<int, int> m;
    while (~scanf("%d%d", &u, &v)) {
        m[u] = v;
    }

    int x[200] = {588045,946274,839998,466040,218545,1091459,1081557,100593,307442,198353,885292,137239,853634,503711,301856,1107688,1098067,181729,1037791,902153,5418,725807,265710,962189,754930,629280,197559,1058248,359381,777879,411751,364603,1108219,856242,521578,123915,60958,803222,910363,157243,699486,812705,507602,314558,424372,1126631,105063,476609,955278,494975,280544,645669,488497,618029,1061451,517442,919166,141439,842582,340248,874534,1088688,535859,152400,1041081,533945,31331,675997,63646,566533,1089984,37542,307337,521376,62395,1127690,1021955,651097,987883,889814,82385,437866,956827,608010,1026426,750246,1082423,168043,1115329,156840,404606,844509,149085,99248,836990,476030,295691,718895,403153,916459,816001,727944,612548,606511,1097616,118565,961453,681106,812121,851704,607181,485574,91534,381086,446567,979701,98147,1088998,764127,270723,1028357,149140,235134,317476,271125,229481,620336,434047,862195,443493,336915,521077,491965,1110239,3871,484714,136840,1014690,520717,825216,880359,284529,535014,1127453,585691,662065,854373,624474,487724,855674,457228,227673,713156,1039499,850675,359998,44204,1058776,120156,946350,823533,145657,527199,292346,50868,868383,948024,138384,104628,336197,1013038,218223,581869,894490,509500,661613,431385,268325,651029,763547,303659,11088,794731,368902,880924,1051555,816081,781435,1114400,471567,877225,953953,1044096,746032,95457,696093,373305,856955,96909,38743};

    for (int i = 0; i < 200; ++i) {
        printf("%d\n", m[x[i]]);
        if (i % 20 == 19) printf("\n");
    }
    return 0;
}
