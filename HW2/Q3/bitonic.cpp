#include <mpi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Helper to check if a number is a power of 2
static inline bool is_power_of_two(long long n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank = 0, num_procs = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Validate that number of processes is a power of 2
    if (!is_power_of_two(num_procs)) {
        if (rank == 0) {
            std::cerr << "Error: Number of processes (" << num_procs << ") must be a power of 2 (e.g., 1, 2, 4, 8)." << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    long long N = 0;
    std::vector<int> global_data;
    std::vector<int> original_copy;
    bool benchmark_mode = false;
    bool print_output = false;
    unsigned long long seed = 42;

    if (argc < 2) {
        if (rank == 0) {
            std::cerr << "Usage:" << std::endl;
            std::cerr << "  " << argv[0] << " <input_file> [--benchmark] [--print]" << std::endl;
            std::cerr << "  " << argv[0] << " <N> [seed] [--benchmark] [--print]" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Parse options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--benchmark") {
            benchmark_mode = true;
        } else if (arg == "--print") {
            print_output = true;
        }
    }

    // Rank 0 loads or generates data
    if (rank == 0) {
        std::string first_arg = argv[1];
        // Check if first_arg is a number or a file
        bool is_number = true;
        for (char c : first_arg) {
            if (!std::isdigit(c)) {
                is_number = false;
                break;
            }
        }

        if (is_number) {
            N = std::stoll(first_arg);
            if (argc >= 3 && std::string(argv[2]).rfind("--", 0) != 0) {
                seed = std::stoull(argv[2]);
            }
            if (!is_power_of_two(N)) {
                std::cerr << "Error: N (" << N << ") must be a power of 2." << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            if (N < num_procs) {
                std::cerr << "Error: N (" << N << ") must be >= number of processes (" << num_procs << ")." << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            std::mt19937_64 rng(seed);
            std::uniform_int_distribution<int> dist(-1000000, 1000000);
            global_data.resize(N);
            for (long long i = 0; i < N; ++i) {
                global_data[i] = dist(rng);
            }
        } else {
            // Load from file
            std::ifstream infile(first_arg);
            if (!infile) {
                std::cerr << "Error: Could not open file " << first_arg << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            // First line can be N or simply elements
            infile >> N;
            if (!is_power_of_two(N) || N < num_procs) {
                std::cerr << "Error: N (" << N << ") must be a power of 2 and >= P (" << num_procs << ")." << std::endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            global_data.resize(N);
            for (long long i = 0; i < N; ++i) {
                infile >> global_data[i];
            }
        }

        original_copy = global_data;
    }

    // Broadcast N to all processes
    MPI_Bcast(&N, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    int local_n = static_cast<int>(N / num_procs);
    std::vector<int> local_data(local_n);
    std::vector<int> recv_data(local_n);
    std::vector<int> next_data(local_n);

    double comm_time = 0.0;
    double comp_time = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    double total_start = MPI_Wtime();

    // 1. Scatter data to all processes
    double t0 = MPI_Wtime();
    MPI_Scatter(global_data.data(), local_n, MPI_INT,
                local_data.data(), local_n, MPI_INT,
                0, MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    comm_time += (t1 - t0);

    // 2. Initial Local Sort
    // When building bitonic sequences:
    // Even ranks sort ascending, odd ranks sort descending in Stage 1 blocks.
    // In general: stage 1 direction is determined by (rank >> 1) & 1.
    t0 = MPI_Wtime();
    if (num_procs == 1) {
        std::sort(local_data.begin(), local_data.end());
    } else {
        bool init_asc = (((rank >> 1) & 1) == (rank & 1));
        if (init_asc) {
            std::sort(local_data.begin(), local_data.end());
        } else {
            std::sort(local_data.begin(), local_data.end(), std::greater<int>());
        }
    }
    t1 = MPI_Wtime();
    comp_time += (t1 - t0);

    // 3. Bitonic Merge Network across processes
    int dimensions = 0;
    while ((1 << dimensions) < num_procs) {
        dimensions++;
    }

    for (int stage = 1; stage <= dimensions; ++stage) {
        // Target direction for current process in outer stage
        int dir = ((rank >> stage) & 1) == 0 ? 1 : 0; // 1 = Ascending, 0 = Descending

        for (int step = stage - 1; step >= 0; --step) {
            int partner = rank ^ (1 << step);
            bool is_lower = (rank < partner);
            bool keep_min = (is_lower == (dir == 1));

            // Communication: Exchange chunks with partner
            t0 = MPI_Wtime();
            MPI_Sendrecv(local_data.data(), local_n, MPI_INT, partner, 0,
                         recv_data.data(), local_n, MPI_INT, partner, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            t1 = MPI_Wtime();
            comm_time += (t1 - t0);

            // Computation: Position-wise compare and retain required half
            t0 = MPI_Wtime();
            if (keep_min) {
                for (int i = 0; i < local_n; ++i) {
                    next_data[i] = std::min(local_data[i], recv_data[i]);
                }
            } else {
                for (int i = 0; i < local_n; ++i) {
                    next_data[i] = std::max(local_data[i], recv_data[i]);
                }
            }
            local_data = next_data;

            // Re-sort locally in stage direction
            if (dir == 1) {
                std::sort(local_data.begin(), local_data.end());
            } else {
                std::sort(local_data.begin(), local_data.end(), std::greater<int>());
            }
            t1 = MPI_Wtime();
            comp_time += (t1 - t0);
        }
    }

    // 4. Gather sorted chunks back to Rank 0
    t0 = MPI_Wtime();
    std::vector<int> sorted_result;
    if (rank == 0) {
        sorted_result.resize(N);
    }
    MPI_Gather(local_data.data(), local_n, MPI_INT,
               sorted_result.data(), local_n, MPI_INT,
               0, MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    comm_time += (t1 - t0);

    MPI_Barrier(MPI_COMM_WORLD);
    double total_end = MPI_Wtime();
    double total_time = total_end - total_start;

    // Reduce max times across all ranks
    double max_total_time = 0.0, max_comm_time = 0.0, max_comp_time = 0.0;
    MPI_Reduce(&total_time, &max_total_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&comm_time, &max_comm_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&comp_time, &max_comp_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        // Sequential correctness verification
        double seq_start = MPI_Wtime();
        std::sort(original_copy.begin(), original_copy.end());
        double seq_end = MPI_Wtime();
        double seq_time = seq_end - seq_start;

        bool is_correct = (sorted_result == original_copy);

        if (print_output || (!benchmark_mode && N <= 64)) {
            for (long long i = 0; i < N; ++i) {
                std::cout << sorted_result[i] << (i + 1 == N ? "" : " ");
            }
            std::cout << std::endl;
        }

        if (benchmark_mode) {
            double comm_pct = (max_total_time > 0.0) ? (max_comm_time / max_total_time) * 100.0 : 0.0;
            double comp_pct = (max_total_time > 0.0) ? (max_comp_time / max_total_time) * 100.0 : 0.0;
            std::cout << std::fixed << std::setprecision(6);
            std::cout << "N=" << N
                      << " P=" << num_procs
                      << " SeqTime=" << seq_time
                      << " TotalTime=" << max_total_time
                      << " CompTime=" << max_comp_time << " (" << std::setprecision(2) << comp_pct << "%)"
                      << " CommTime=" << max_comm_time << " (" << std::setprecision(2) << comm_pct << "%)"
                      << " Correct=" << (is_correct ? "YES" : "NO")
                      << std::endl;
        } else if (N > 64 && !print_output) {
            std::cout << "Bitonic Sort Complete. N = " << N << ", P = " << num_procs
                      << ", Correctness: " << (is_correct ? "PASSED" : "FAILED")
                      << ", Total Time: " << max_total_time << "s" << std::endl;
        }

        if (!is_correct) {
            std::cerr << "Verification failed: output does not match sequential std::sort!" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Finalize();
    return 0;
}
