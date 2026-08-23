#include<bits/stdc++.h>
#include<mpi.h>
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
    MPI_Init(&argc, &argv);
    fast;
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) {
            cout << "You need to provide exactly 1 argument: the filename." << endl;
        }
        MPI_Finalize();
        return 1;
    }

    string filename = argv[1];
    int v, e;
    vector<int> degree;
    vector<pair<int, int>> edges;

    if (rank == 0) {
        ifstream infile(filename);
        if (!infile) {
            cout << "Error opening file: " << filename << endl;

            MPI_Abort(MPI_COMM_WORLD, 1);

        }
        infile >> v >> e;
        degree.resize(v, 0);
        edges.resize(e);
        for (int i = 0; i < e; i++) {
            int u, w;
            infile >> u >> w;
            edges[i] = {u, w};
            degree[u]++;
            degree[w]++;
        }
        infile.close();
    }

    MPI_Bcast(&v, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&e, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        degree.resize(v);
    }

    MPI_Bcast(degree.data(), v, MPI_INT, 0, MPI_COMM_WORLD );

    vector<vector<int>> adj;
    if (rank == 0) {
        adj.resize(v);
        for (auto edge : edges) {
            int u = edge.first;
            int w = edge.second;
            if (degree[u] < degree[w] || (degree[u] == degree[w] && u < w)) {
                adj[u].pb(w);
            } else {
                adj[w].pb(u);
            }
        }
        for (int i = 0; i < v; i++) {
            st(adj[i]);
        }
    }

    vector<int> adj_size(v);
    if (rank == 0) {
        for (int i = 0; i < v; i++) {
            adj_size[i] = adj[i].size();
        }
    }

    MPI_Bcast(adj_size.data(), v, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> flat_adj;
    vector<int> adj_start(v + 1, 0);
    if (rank == 0) {
        for (int i = 0; i < v; i++) {
            adj_start[i + 1] = adj_start[i] + adj_size[i];
            for (int x : adj[i]) {
                flat_adj.pb(x);
            }
        }
    }

    MPI_Bcast(adj_start.data(), v + 1, MPI_INT, 0, MPI_COMM_WORLD);

    int total_adj_size = 0;

    if (rank == 0) {
        total_adj_size = flat_adj.size();
    }

    MPI_Bcast(&total_adj_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        flat_adj.resize(total_adj_size);
    }

    MPI_Bcast(flat_adj.data(), total_adj_size, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        adj.resize(v);
        for (int i = 0; i < v; i++) {
            for (int j = adj_start[i]; j < adj_start[i + 1]; j++) {
                adj[i].pb(flat_adj[j]);
            }
        }
    }

    vector<int> send_counts(size);
    vector<int> displacements(size);
    int base = e / size;
    int rem = e % size;
    for (int i = 0; i < size; i++) {
        send_counts[i] = base;
        if (i < rem)
            send_counts[i]++;
    }
    displacements[0] = 0;
    for (int i = 1; i < size; i++) {
        displacements[i] = displacements[i - 1] + send_counts[i - 1];
    }

    vector<int> flat_edges;
    if (rank == 0) {
        flat_edges.resize(2 * e);
        for (int i = 0; i < e; i++) {
            flat_edges[2 * i] = edges[i].first;
            flat_edges[2 * i + 1] = edges[i].second;
        }
    }

    vector<int> mpi_counts(size);
    vector<int> mpi_displacements(size);
    for (int i = 0; i < size; i++) {
        mpi_counts[i] = send_counts[i] * 2;
        mpi_displacements[i] = displacements[i] * 2;
    }

    vector<int> local_edges(send_counts[rank] * 2);

    MPI_Scatterv(flat_edges.data(), mpi_counts.data(), mpi_displacements.data(), MPI_INT, local_edges.data(), mpi_counts[rank], MPI_INT, 0, MPI_COMM_WORLD);

    ll local_triangles = 0;
    for (int i = 0; i < send_counts[rank]; i++) {
        int u = local_edges[2 * i];
        int w = local_edges[2 * i + 1];
        if (degree[u] < degree[w] || (degree[u] == degree[w] && u < w)) {
            for (int x : adj[u]) {
                if (binary_search(adj[w].begin(), adj[w].end(), x)) {
                    local_triangles++;
                }
            }
        }
        else{
            for (int x : adj[w]) {
                if (binary_search(adj[u].begin(), adj[u].end(), x)) {
                    local_triangles++;
                }
            }
        }
    }

    ll global_triangles = 0;

    MPI_Reduce(&local_triangles, &global_triangles, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        cout << global_triangles << endl;
    }

    MPI_Finalize();

    return 0;
}