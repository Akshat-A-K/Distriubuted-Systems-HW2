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

ll count_common(const vector<int> &a, const vector<int> &b) {
    ll common = 0;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            common++;
            i++;
            j++;
        } else if (a[i] < b[j]) {
            i++;
        } else {
            j++;
        }
    }
    return common;
}

int main(int argc, char *argv[]) {
    fast;
    if (argc != 2) {
        cout << "You need to provide exactly 1 argument: the filename." << endl;  
        return 1;
    }
    string filename = argv[1];
    ifstream infile(filename);
    if (!infile) {
        cout << "Error opening file: " << filename << endl;
        return 1;
    }
    int v, e;
    infile >> v >> e;
    
    vector<int> degree(v, 0);
    vector<pair<int, int>> edges;
    for (int i = 0; i < e; ++i) {
        int u, w;
        infile >> u >> w;
        edges.push_back({u, w});
        degree[u]++;
        degree[w]++;
    }
    vector<vector<int>> adj(v);
    for (auto e : edges) {
        int u = e.first, v = e.second;

        if (degree[u] < degree[v] || (degree[u] == degree[v] && u < v)) {
            adj[u].push_back(v);
        } else {
            adj[v].push_back(u);
        }
    }

    for (int i = 0; i < v; ++i) {
        sort(all(adj[i]));
    }
    ll triangles = 0;

    for (auto e : edges) {
        int u = e.first, v = e.second;
        if (degree[u] < degree[v] || (degree[u] == degree[v] && u < v)) {
            triangles += count_common(adj[u], adj[v]);
        } else {
            triangles += count_common(adj[v], adj[u]);
        }
    }
    cout << triangles << endl;
    return 0;
}