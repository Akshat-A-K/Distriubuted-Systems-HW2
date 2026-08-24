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
    MPI_Init(&argc, &argv);
    fast;
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double setup_start = MPI_Wtime();

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

    double setup_time = MPI_Wtime() - setup_start;

    if (rank != 0) {
        degree.resize(v);
    }

    double degree_start = MPI_Wtime();
    MPI_Bcast(degree.data(), v, MPI_INT, 0, MPI_COMM_WORLD );
    double degree_time = MPI_Wtime() - degree_start;

    vector<vector<int>> adj;
    if (rank == 0) {
        adj.resize(v);
        for (auto e : edges) {
            int u = e.first, v = e.second;
            if (degree[u] < degree[v] || (degree[u] == degree[v] && u < v)) {
                adj[u].pb(v);
            } else {
                adj[v].pb(u);
            }
        }
        for (int i = 0; i < v; i++) {
            sort(all(adj[i]));
        }
    }

    vector<int> adj_size(v);
    if (rank == 0) {
        for (int i = 0; i < v; i++) {
            adj_size[i] = adj[i].size();
        }
    }

    double adjacency_start = MPI_Wtime();
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
    double adjacency_time = MPI_Wtime() - adjacency_start;

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

    double scatter_start = MPI_Wtime();
    MPI_Scatterv(flat_edges.data(), mpi_counts.data(), mpi_displacements.data(), MPI_INT, local_edges.data(), mpi_counts[rank], MPI_INT, 0, MPI_COMM_WORLD);
    double scatter_time = MPI_Wtime() - scatter_start;

    double compute_start = MPI_Wtime();
    ll local_triangles = 0;
    for (int i = 0; i < send_counts[rank]; i++) {
        int u = local_edges[2 * i];
        int w = local_edges[2 * i + 1];
        if (degree[u] < degree[w] || (degree[u] == degree[w] && u < w)) {
            local_triangles += count_common(adj[u], adj[w]);
        } else {
            local_triangles += count_common(adj[w], adj[u]);
        }
    }
    double compute_time = MPI_Wtime() - compute_start;

    ll global_triangles = 0;
    double reduce_start = MPI_Wtime();
    MPI_Reduce(&local_triangles, &global_triangles, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    double reduce_time = MPI_Wtime() - reduce_start;

    double maximum_setup_time = 0;
    double maximum_degree_time = 0;
    double maximum_adjacency_time = 0;
    double maximum_scatter_time = 0;
    double maximum_compute_time = 0;
    double maximum_reduce_time = 0;

    MPI_Reduce(&setup_time, &maximum_setup_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&degree_time, &maximum_degree_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&adjacency_time, &maximum_adjacency_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&scatter_time, &maximum_scatter_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&compute_time, &maximum_compute_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&reduce_time, &maximum_reduce_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        cout << global_triangles << endl;
        double algorithm_time = maximum_setup_time + maximum_degree_time
            + maximum_adjacency_time + maximum_scatter_time
            + maximum_compute_time + maximum_reduce_time;
        cerr << "MPI_PHASES setup=" << maximum_setup_time
             << " degree=" << maximum_degree_time
             << " adjacency=" << maximum_adjacency_time
             << " scatter=" << maximum_scatter_time
             << " compute=" << maximum_compute_time
             << " reduce=" << maximum_reduce_time
             << " algo=" << algorithm_time << endl;
    }

    MPI_Finalize();

    return 0;
}