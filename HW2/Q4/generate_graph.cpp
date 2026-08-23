#include<bits/stdc++.h>
#define fast ios_base::sync_with_stdio(false);cin.tie(NULL);cout.tie(NULL)
#define pb push_back
#define ll long long
#define inf 0x7fffffff
#define mod 998244353
#define N 100010
#define pp pair<ll,ll>
#define yes cout<<"YES"<<endl
#define no cout<<"NO"<<endl
#define lcm(a,b) ((a*b)/(__gcd(a,b)))
#define all(s) s.begin(), s.end()
#define st(s) sort(all(s))
#define loop(i,n) for(ll i=0; i<n; i++)
using namespace std;

int main(int argc, char *argv[]) {
    fast;
    if (argc != 4) {
        cout << "You need to provide exactly 3 arguments: number of vertices, number of edges, and seed value." << endl;  
        return 1;
    }
    int v = stoi(argv[1]);
    int e = stoi(argv[2]);
    unsigned long long seed = stoull(argv[3]);
    if (v < 3 || v > 100000) {
        cout << "Number of vertices must be between 3 and 100000." << endl;
        return 1;
    }
    if (e < 3 || e > 1000000) {
        cout << "Number of edges must be between 3 and 1000000." << endl;
        return 1;
    }
    long long max_edges = (long long)v * (v - 1) / 2;
    if (e > max_edges) {
        cout << "Number of edges cannot exceed " << max_edges << " for " << v << " vertices." << endl;
        return 1;
    }
    mt19937_64 rng(seed);
    
    unordered_set<long long> edges;

    edges.insert(0LL * v + 1);
    edges.insert(1LL * v + 2);
    edges.insert(0LL * v + 2);

    while (edges.size() < e) {
        int u = rng() % v;
        int w = rng() % v;
        if (u == w)
            continue;
        if (u > w)
            swap(u, w);
        long long edge = 1LL * u * v + w;
        edges.insert(edge);
    }
    vector<pair<int, int>> graph;

    for (auto e : edges) {
        int u = e / v;
        int w = e % v;
        graph.pb({u, w});
    }

    sort(all(graph));

    cout << v << " " << e << endl;

    for (auto e : graph) {
        cout << e.first << " " << e.second << endl;
    }
    return 0;
}