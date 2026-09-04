#include <mpi.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <vector>

using namespace std;
using ll = long long;

static bool forward_edge(int u, int v, const vector<int>& degree) {
    return degree[u] < degree[v] || (degree[u] == degree[v] && u < v);
}

static ll count_common(const vector<int>& a, const vector<int>& b) {
    ll count = 0;
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) { ++count; ++i; ++j; }
        else if (a[i] < b[j]) ++i;
        else ++j;
    }
    return count;
}

static vector<int> prefix_displacements(const vector<int>& counts) {
    vector<int> displacements(counts.size(), 0);
    for (size_t i = 1; i < counts.size(); ++i) {
        displacements[i] = displacements[i - 1] + counts[i - 1];
    }
    return displacements;
}

static int owner_of(int vertex, int vertices, int processes) {
    return min(processes - 1, vertex * processes / vertices);
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank = 0;
    int processes = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);

    if (argc != 2) {
        if (rank == 0) cout << "Usage: " << argv[0] << " <filename>\n";
        MPI_Finalize();
        return 1;
    }

    double setup_start = MPI_Wtime();
    int vertices = 0;
    int edge_count = 0;
    vector<int> degree;
    vector<int> flat_edges;
    if (rank == 0) {
        ifstream input(argv[1]);
        if (!(input >> vertices >> edge_count) || vertices < 1 || edge_count < 0) {
            cout << "Error: invalid graph header.\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        degree.assign(vertices, 0);
        flat_edges.resize(2 * edge_count);
        for (int i = 0; i < edge_count; ++i) {
            int u = 0;
            int v = 0;
            if (!(input >> u >> v) || u < 0 || v < 0 || u >= vertices || v >= vertices || u == v) {
                cout << "Error: invalid edge at index " << i << ".\n";
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            flat_edges[2 * i] = u;
            flat_edges[2 * i + 1] = v;
            ++degree[u];
            ++degree[v];
        }
    }

    MPI_Bcast(&vertices, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&edge_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) degree.resize(vertices);
    MPI_Bcast(degree.data(), vertices, MPI_INT, 0, MPI_COMM_WORLD);
    double setup_time = MPI_Wtime() - setup_start;

    vector<int> edge_counts(processes, edge_count / processes);
    for (int p = 0; p < edge_count % processes; ++p) ++edge_counts[p];
    vector<int> edge_displacements = prefix_displacements(edge_counts);
    vector<int> edge_counts_x2(processes);
    vector<int> edge_displacements_x2(processes);
    for (int p = 0; p < processes; ++p) {
        edge_counts_x2[p] = 2 * edge_counts[p];
        edge_displacements_x2[p] = 2 * edge_displacements[p];
    }

    vector<int> local_edges(edge_counts_x2[rank]);
    double scatter_start = MPI_Wtime();
    MPI_Scatterv(rank == 0 ? flat_edges.data() : nullptr, edge_counts_x2.data(), edge_displacements_x2.data(), MPI_INT,
                 local_edges.data(), edge_counts_x2[rank], MPI_INT, 0, MPI_COMM_WORLD);
    double scatter_time = MPI_Wtime() - scatter_start;

    vector<vector<int>> root_adjacency;
    vector<int> all_adjacency_sizes;
    if (rank == 0) {
        root_adjacency.resize(vertices);
        for (int i = 0; i < edge_count; ++i) {
            int u = flat_edges[2 * i];
            int v = flat_edges[2 * i + 1];
            if (forward_edge(u, v, degree)) root_adjacency[u].push_back(v);
            else root_adjacency[v].push_back(u);
        }
        all_adjacency_sizes.resize(vertices);
        for (int vertex = 0; vertex < vertices; ++vertex) {
            sort(root_adjacency[vertex].begin(), root_adjacency[vertex].end());
            all_adjacency_sizes[vertex] = static_cast<int>(root_adjacency[vertex].size());
        }
    }

    vector<int> vertex_counts(processes);
    vector<int> vertex_displacements(processes);
    for (int p = 0; p < processes; ++p) {
        vertex_displacements[p] = vertices * p / processes;
        vertex_counts[p] = vertices * (p + 1) / processes - vertex_displacements[p];
    }
    int first_vertex = vertex_displacements[rank];
    int local_vertex_count = vertex_counts[rank];
    vector<int> local_sizes(local_vertex_count);

    double adjacency_start = MPI_Wtime();
    MPI_Scatterv(rank == 0 ? all_adjacency_sizes.data() : nullptr,
                 vertex_counts.data(), vertex_displacements.data(), MPI_INT,
                 local_sizes.data(), local_vertex_count, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> adjacency_counts(processes, 0);
    vector<int> adjacency_displacements;
    vector<int> all_flat_adjacency;
    if (rank == 0) {
        for (int p = 0; p < processes; ++p) {
            int begin = vertex_displacements[p];
            int end = begin + vertex_counts[p];
            for (int vertex = begin; vertex < end; ++vertex) adjacency_counts[p] += all_adjacency_sizes[vertex];
        }
        adjacency_displacements = prefix_displacements(adjacency_counts);
        all_flat_adjacency.reserve(edge_count);
        for (const auto& list : root_adjacency) all_flat_adjacency.insert(all_flat_adjacency.end(), list.begin(), list.end());
    }
    MPI_Bcast(adjacency_counts.data(), processes, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) adjacency_displacements = prefix_displacements(adjacency_counts);
    int local_adjacency_count = accumulate(local_sizes.begin(), local_sizes.end(), 0);
    vector<int> local_flat_adjacency(local_adjacency_count);
    MPI_Scatterv(rank == 0 ? all_flat_adjacency.data() : nullptr,
                 adjacency_counts.data(), adjacency_displacements.data(), MPI_INT,
                 local_flat_adjacency.data(), local_adjacency_count, MPI_INT, 0, MPI_COMM_WORLD);

    unordered_map<int, vector<int>> owned_adjacency;
    int local_offset = 0;
    for (int vertex = first_vertex; vertex < first_vertex + local_vertex_count; ++vertex) {
        int length = local_sizes[vertex - first_vertex];
        owned_adjacency[vertex] = vector<int>(local_flat_adjacency.begin() + local_offset,
                                              local_flat_adjacency.begin() + local_offset + length);
        local_offset += length;
    }

    vector<int> needed_vertices;
    for (int i = 0; i < edge_counts[rank]; ++i) {
        needed_vertices.push_back(local_edges[2 * i]);
        needed_vertices.push_back(local_edges[2 * i + 1]);
    }
    sort(needed_vertices.begin(), needed_vertices.end());
    needed_vertices.erase(unique(needed_vertices.begin(), needed_vertices.end()), needed_vertices.end());

    vector<int> request_counts(processes, 0);
    for (int vertex : needed_vertices) ++request_counts[owner_of(vertex, vertices, processes)];
    vector<int> request_displacements = prefix_displacements(request_counts);
    vector<int> requests(needed_vertices.size());
    vector<int> request_offsets = request_displacements;
    for (int vertex : needed_vertices) {
        int owner = owner_of(vertex, vertices, processes);
        requests[request_offsets[owner]++] = vertex;
    }

    vector<int> incoming_request_counts(processes);
    MPI_Alltoall(request_counts.data(), 1, MPI_INT, incoming_request_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    vector<int> incoming_request_displacements = prefix_displacements(incoming_request_counts);
    int incoming_total = accumulate(incoming_request_counts.begin(), incoming_request_counts.end(), 0);
    vector<int> incoming_requests(incoming_total);
    MPI_Alltoallv(requests.data(), request_counts.data(), request_displacements.data(), MPI_INT,
                  incoming_requests.data(), incoming_request_counts.data(), incoming_request_displacements.data(), MPI_INT,
                  MPI_COMM_WORLD);

    vector<int> response_counts(processes, 0);
    for (int p = 0; p < processes; ++p) {
        for (int i = incoming_request_displacements[p]; i < incoming_request_displacements[p] + incoming_request_counts[p]; ++i)
            response_counts[p] += 1 + static_cast<int>(owned_adjacency[incoming_requests[i]].size());
    }
    vector<int> response_displacements = prefix_displacements(response_counts);
    vector<int> responses(accumulate(response_counts.begin(), response_counts.end(), 0));
    for (int p = 0; p < processes; ++p) {
        int position = response_displacements[p];
        for (int i = incoming_request_displacements[p]; i < incoming_request_displacements[p] + incoming_request_counts[p]; ++i) {
            const auto& list = owned_adjacency[incoming_requests[i]];
            responses[position++] = static_cast<int>(list.size());
            copy(list.begin(), list.end(), responses.begin() + position);
            position += static_cast<int>(list.size());
        }
    }

    vector<int> returned_counts(processes);
    MPI_Alltoall(response_counts.data(), 1, MPI_INT, returned_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    vector<int> returned_displacements = prefix_displacements(returned_counts);
    int returned_total = accumulate(returned_counts.begin(), returned_counts.end(), 0);
    vector<int> returned(returned_total);
    MPI_Alltoallv(responses.data(), response_counts.data(), response_displacements.data(), MPI_INT,
                  returned.data(), returned_counts.data(), returned_displacements.data(), MPI_INT, MPI_COMM_WORLD);

    unordered_map<int, vector<int>> adjacency;
    for (int p = 0; p < processes; ++p) {
        int position = returned_displacements[p];
        for (int i = request_displacements[p]; i < request_displacements[p] + request_counts[p]; ++i) {
            int vertex = requests[i];
            int length = returned[position++];
            adjacency[vertex] = vector<int>(returned.begin() + position, returned.begin() + position + length);
            position += length;
        }
    }
    double adjacency_time = MPI_Wtime() - adjacency_start;

    double compute_start = MPI_Wtime();
    ll local_triangles = 0;
    for (int i = 0; i < edge_counts[rank]; ++i) {
        int u = local_edges[2 * i];
        int v = local_edges[2 * i + 1];
        if (forward_edge(u, v, degree)) local_triangles += count_common(adjacency[u], adjacency[v]);
        else local_triangles += count_common(adjacency[v], adjacency[u]);
    }
    double compute_time = MPI_Wtime() - compute_start;

    ll global_triangles = 0;
    double reduce_start = MPI_Wtime();
    MPI_Reduce(&local_triangles, &global_triangles, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    double reduce_time = MPI_Wtime() - reduce_start;

    double max_setup = 0.0, max_scatter = 0.0, max_adjacency = 0.0, max_compute = 0.0, max_reduce = 0.0;
    MPI_Reduce(&setup_time, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&scatter_time, &max_scatter, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&adjacency_time, &max_adjacency, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&compute_time, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&reduce_time, &max_reduce, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        cout << global_triangles << '\n';
        double algorithm_time = max_setup + max_scatter + max_adjacency + max_compute + max_reduce;
        cerr << "MPI_PHASES setup=" << max_setup
             << " degree=0"
             << " adjacency=" << max_adjacency
             << " scatter=" << max_scatter
             << " compute=" << max_compute
             << " reduce=" << max_reduce
             << " algo=" << algorithm_time << '\n';
    }

    MPI_Finalize();
    return 0;
}
