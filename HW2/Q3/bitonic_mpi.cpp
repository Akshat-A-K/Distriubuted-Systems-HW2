#include <mpi.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

static inline bool is_power_of_two(long long n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double setup_start = MPI_Wtime();

    if (!is_power_of_two(size)) {
        if (rank == 0) {
            cerr << "Error: Number of processes (" << size << ") must be a power of 2 (1, 2, 4, 8)." << endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (argc != 2) {
        if (rank == 0) {
            cerr << "Usage: " << argv[0] << " <filename>" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    string filename = argv[1];
    long long n = 0;
    vector<int> global_data;

    if (rank == 0) {
        ifstream infile(filename);
        if (!infile) {
            cerr << "Error opening file: " << filename << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (!(infile >> n)) {
            cerr << "Error reading N from file: " << filename << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (!is_power_of_two(n) || n < size) {
            cerr << "Error: N (" << n << ") must be a power of 2 and >= P (" << size << ")." << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        global_data.resize(n);
        for (long long i = 0; i < n; ++i) {
            if (!(infile >> global_data[i])) {
                cerr << "Error reading data element at index " << i << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        infile.close();
    }

    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    double setup_time = MPI_Wtime() - setup_start;

    int local_n = static_cast<int>(n / size);
    vector<int> local_data(local_n);
    vector<int> recv_data(local_n);
    vector<int> next_data(local_n);

    // 1. Scatter data
    double scatter_start = MPI_Wtime();
    MPI_Scatter(global_data.data(), local_n, MPI_INT,
                local_data.data(), local_n, MPI_INT,
                0, MPI_COMM_WORLD);
    double scatter_time = MPI_Wtime() - scatter_start;

    // 2. Initial Local Sort
    double initsort_start = MPI_Wtime();
    if (size == 1) {
        sort(local_data.begin(), local_data.end());
    } else {
        bool init_asc = (((rank >> 1) & 1) == (rank & 1));
        if (init_asc) {
            sort(local_data.begin(), local_data.end());
        } else {
            sort(local_data.begin(), local_data.end(), greater<int>());
        }
    }
    double initsort_time = MPI_Wtime() - initsort_start;

    // 3. Bitonic Merge Network
    int dimensions = 0;
    while ((1 << dimensions) < size) {
        dimensions++;
    }

    double stagecomm_time = 0.0;
    double stagecomp_time = 0.0;

    for (int stage = 1; stage <= dimensions; ++stage) {
        int dir = ((rank >> stage) & 1) == 0 ? 1 : 0; // 1 = Ascending, 0 = Descending

        for (int step = stage - 1; step >= 0; --step) {
            int partner = rank ^ (1 << step);
            bool is_lower = (rank < partner);
            bool keep_min = (is_lower == (dir == 1));

            // Exchange chunk with partner
            double t0 = MPI_Wtime();
            MPI_Sendrecv(local_data.data(), local_n, MPI_INT, partner, 0,
                         recv_data.data(), local_n, MPI_INT, partner, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double t1 = MPI_Wtime();
            stagecomm_time += (t1 - t0);

            // Compare-exchange
            t0 = MPI_Wtime();
            if (keep_min) {
                for (int i = 0; i < local_n; ++i) {
                    next_data[i] = min(local_data[i], recv_data[i]);
                }
            } else {
                for (int i = 0; i < local_n; ++i) {
                    next_data[i] = max(local_data[i], recv_data[i]);
                }
            }
            local_data = next_data;

            if (dir == 1) {
                sort(local_data.begin(), local_data.end());
            } else {
                sort(local_data.begin(), local_data.end(), greater<int>());
            }
            t1 = MPI_Wtime();
            stagecomp_time += (t1 - t0);
        }
    }

    // 4. Gather sorted chunks back to Rank 0
    double gather_start = MPI_Wtime();
    vector<int> sorted_result;
    if (rank == 0) {
        sorted_result.resize(n);
    }
    MPI_Gather(local_data.data(), local_n, MPI_INT,
               sorted_result.data(), local_n, MPI_INT,
               0, MPI_COMM_WORLD);
    double gather_time = MPI_Wtime() - gather_start;

    // Reduce maximum phase times across all ranks
    double max_setup_time = 0.0;
    double max_scatter_time = 0.0;
    double max_initsort_time = 0.0;
    double max_stagecomm_time = 0.0;
    double max_stagecomp_time = 0.0;
    double max_gather_time = 0.0;

    MPI_Reduce(&setup_time, &max_setup_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&scatter_time, &max_scatter_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&initsort_time, &max_initsort_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&stagecomm_time, &max_stagecomm_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&stagecomp_time, &max_stagecomp_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&gather_time, &max_gather_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        // Output sorted array to stdout
        for (long long i = 0; i < n; ++i) {
            cout << sorted_result[i] << (i + 1 == n ? "" : " ");
        }
        cout << "\n";

        // Aggregate timing metrics
        double compute_time = max_initsort_time + max_stagecomp_time;
        double comm_time = max_scatter_time + max_stagecomm_time + max_gather_time;
        double algo_time = max_setup_time + max_scatter_time + max_initsort_time
                         + max_stagecomm_time + max_stagecomp_time + max_gather_time;

        cerr << "MPI_PHASES setup=" << max_setup_time
             << " scatter=" << max_scatter_time
             << " initsort=" << max_initsort_time
             << " stagecomm=" << max_stagecomm_time
             << " stagecomp=" << max_stagecomp_time
             << " gather=" << max_gather_time
             << " compute=" << compute_time
             << " comm=" << comm_time
             << " algo=" << algo_time << endl;
    }

    MPI_Finalize();
    return 0;
}
