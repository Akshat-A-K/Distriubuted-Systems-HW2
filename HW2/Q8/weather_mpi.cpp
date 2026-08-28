#include <mpi.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Record {
    long long timestamp;
    int station_id;
    double temperature;
    double humidity;
    double pressure;
    double rainfall;
    double wind_speed;
};

// Kahan (compensated) summation -- see weather_seq.cpp for the full
// explanation. Used both for local per-rank accumulation and, critically,
// for combining the P ranks' partial sums on rank 0 (replacing plain
// MPI_Reduce(MPI_SUM) for these fields, which reintroduces the same
// non-associativity problem at the reduction-tree level).
struct KahanSum {
    long double sum = 0.0L;
    long double c = 0.0L;
    void add(double x_in) {
        long double x = (long double)x_in;
        long double y = x - c;
        long double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
};

struct StationAgg {
    long long count = 0;
    KahanSum sum_temp;
    KahanSum sum_rainfall;
};

// Build an MPI derived datatype matching the Record struct's layout so it
// can be scattered directly, instead of manually packing/unpacking fields.
static MPI_Datatype make_record_type() {
    MPI_Datatype record_type;
    int block_lengths[7] = {1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[7];
    MPI_Datatype types[7] = {MPI_LONG_LONG, MPI_INT, MPI_DOUBLE, MPI_DOUBLE,
                              MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE};

    Record probe{};
    MPI_Aint base_address;
    MPI_Get_address(&probe, &base_address);
    MPI_Get_address(&probe.timestamp, &displacements[0]);
    MPI_Get_address(&probe.station_id, &displacements[1]);
    MPI_Get_address(&probe.temperature, &displacements[2]);
    MPI_Get_address(&probe.humidity, &displacements[3]);
    MPI_Get_address(&probe.pressure, &displacements[4]);
    MPI_Get_address(&probe.rainfall, &displacements[5]);
    MPI_Get_address(&probe.wind_speed, &displacements[6]);
    for (int i = 0; i < 7; ++i) {
        displacements[i] -= base_address;
    }

    MPI_Type_create_struct(7, block_lengths, displacements, types, &record_type);
    MPI_Type_commit(&record_type);
    return record_type;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double setup_start = MPI_Wtime();

    if (argc != 2) {
        if (rank == 0) cerr << "Usage: " << argv[0] << " <filename>" << endl;
        MPI_Finalize();
        return 1;
    }

    long long n = 0, k = 0, s = 0;
    vector<Record> all_records;

    if (rank == 0) {
        ifstream infile(argv[1]);
        if (!infile) {
            cerr << "Error opening file: " << argv[1] << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (!(infile >> n >> k >> s)) {
            cerr << "Error reading N K S header." << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        all_records.resize(n);
        for (long long i = 0; i < n; ++i) {
            Record &r = all_records[i];
            if (!(infile >> r.timestamp >> r.station_id >> r.temperature >> r.humidity >>
                  r.pressure >> r.rainfall >> r.wind_speed)) {
                cerr << "Error reading record at index " << i << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        infile.close();
    }

    long long header[3];
    if (rank == 0) { header[0] = n; header[1] = k; header[2] = s; }
    MPI_Bcast(header, 3, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    n = header[0]; k = header[1]; s = header[2];

    MPI_Datatype record_type = make_record_type();

    // Partition N across P ranks; N need not be divisible by P, so the
    // first (N % P) ranks get one extra record (standard block partition
    // with remainder handling).
    vector<int> counts(size), displs(size);
    {
        long long base = n / size, rem = n % size;
        long long offset = 0;
        for (int p = 0; p < size; ++p) {
            long long c = base + (p < rem ? 1 : 0);
            counts[p] = (int)c;
            displs[p] = (int)offset;
            offset += c;
        }
    }
    int local_n = counts[rank];
    vector<Record> local_records(local_n);

    double setup_time = MPI_Wtime() - setup_start;

    double scatter_start = MPI_Wtime();
    MPI_Scatterv(rank == 0 ? all_records.data() : nullptr, counts.data(), displs.data(),
                 record_type, local_records.data(), local_n, record_type, 0, MPI_COMM_WORLD);
    double scatter_time = MPI_Wtime() - scatter_start;

    // ---- Local computation ----
    double compute_start = MPI_Wtime();

    KahanSum sum_temp, sum_humidity, sum_pressure, sum_rainfall, sum_wind;
    double min_temp = 1e300, max_temp = -1e300;
    double min_humidity = 1e300, max_humidity = -1e300;
    double min_pressure = 1e300, max_pressure = -1e300;
    double max_rainfall = -1e300;
    double max_wind = -1e300;
    long long extreme_count = 0;

    bool have_hottest = false, have_coldest = false;
    Record hottest{}, coldest{};

    unordered_map<int, StationAgg> station_map;
    unordered_map<long long, long long> interval_map;

    for (int i = 0; i < local_n; ++i) {
        const Record &r = local_records[i];

        sum_temp.add(r.temperature);
        min_temp = min(min_temp, r.temperature);
        max_temp = max(max_temp, r.temperature);

        sum_humidity.add(r.humidity);
        min_humidity = min(min_humidity, r.humidity);
        max_humidity = max(max_humidity, r.humidity);

        sum_pressure.add(r.pressure);
        min_pressure = min(min_pressure, r.pressure);
        max_pressure = max(max_pressure, r.pressure);

        sum_rainfall.add(r.rainfall);
        max_rainfall = max(max_rainfall, r.rainfall);

        sum_wind.add(r.wind_speed);
        max_wind = max(max_wind, r.wind_speed);

        if (r.temperature >= 40.0 || r.temperature <= 0.0) extreme_count++;

        if (!have_hottest || r.temperature > hottest.temperature ||
            (r.temperature == hottest.temperature && r.timestamp < hottest.timestamp) ||
            (r.temperature == hottest.temperature && r.timestamp == hottest.timestamp &&
             r.station_id < hottest.station_id)) {
            hottest = r;
            have_hottest = true;
        }

        if (!have_coldest || r.temperature < coldest.temperature ||
            (r.temperature == coldest.temperature && r.timestamp < coldest.timestamp) ||
            (r.temperature == coldest.temperature && r.timestamp == coldest.timestamp &&
             r.station_id < coldest.station_id)) {
            coldest = r;
            have_coldest = true;
        }

        StationAgg &sa = station_map[r.station_id];
        sa.count++;
        sa.sum_temp.add(r.temperature);
        sa.sum_rainfall.add(r.rainfall);

        interval_map[r.timestamp / 60]++;
    }

    // If a rank got zero local records (possible if N < P), guard the
    // hottest/coldest sentinel values so they never win a real comparison.
    if (!have_hottest) { hottest.temperature = -1e300; hottest.timestamp = 0; hottest.station_id = 0; }
    if (!have_coldest) { coldest.temperature = 1e300; coldest.timestamp = 0; coldest.station_id = 0; }

    double compute_time = MPI_Wtime() - compute_start;

    // ---- Combine across ranks ----
    double comm_start = MPI_Wtime();

    // Each rank's local sum is already Kahan-compensated, but combining P
    // partial sums via plain MPI_Reduce(SUM) still sums them in whatever
    // order the MPI implementation's internal reduction tree picks -- which
    // can differ from run to run and from the sequential program's order,
    // reintroducing drift at this second level. Gather the P partial sums
    // to rank 0 and do one more Kahan pass over just those P values
    // (P is small, so this is cheap) instead.
    double local_sums[5] = {(double)sum_temp.sum, (double)sum_humidity.sum, (double)sum_pressure.sum,
                             (double)sum_rainfall.sum, (double)sum_wind.sum};
    vector<double> gathered_sums;
    if (rank == 0) gathered_sums.resize(5 * size);
    MPI_Gather(local_sums, 5, MPI_DOUBLE, rank == 0 ? gathered_sums.data() : nullptr, 5, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    long long local_extreme = extreme_count, global_extreme = 0;
    MPI_Reduce(&local_extreme, &global_extreme, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double sum_reduced[6] = {0};
    if (rank == 0) {
        KahanSum final_temp, final_humidity, final_pressure, final_rainfall, final_wind;
        for (int p = 0; p < size; ++p) {
            final_temp.add(gathered_sums[5 * p + 0]);
            final_humidity.add(gathered_sums[5 * p + 1]);
            final_pressure.add(gathered_sums[5 * p + 2]);
            final_rainfall.add(gathered_sums[5 * p + 3]);
            final_wind.add(gathered_sums[5 * p + 4]);
        }
        sum_reduced[0] = final_temp.sum;
        sum_reduced[1] = final_humidity.sum;
        sum_reduced[2] = final_pressure.sum;
        sum_reduced[3] = final_rainfall.sum;
        sum_reduced[4] = final_wind.sum;
        sum_reduced[5] = (double)global_extreme;
    }

    double min_pack[3] = {min_temp, min_humidity, min_pressure};
    double min_reduced[3];
    MPI_Reduce(min_pack, min_reduced, 3, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    double max_pack[5] = {max_temp, max_humidity, max_pressure, max_rainfall, max_wind};
    double max_reduced[5];
    MPI_Reduce(max_pack, max_reduced, 5, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Hottest/coldest: gather each rank's local best candidate (P records,
    // always cheap) and resolve the final tie-break on rank 0.
    double local_hot[3] = {hottest.temperature, (double)hottest.timestamp, (double)hottest.station_id};
    double local_cold[3] = {coldest.temperature, (double)coldest.timestamp, (double)coldest.station_id};
    vector<double> hot_gather, cold_gather;
    if (rank == 0) { hot_gather.resize(3 * size); cold_gather.resize(3 * size); }
    MPI_Gather(local_hot, 3, MPI_DOUBLE, rank == 0 ? hot_gather.data() : nullptr, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(local_cold, 3, MPI_DOUBLE, rank == 0 ? cold_gather.data() : nullptr, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Top-station and interval maps: flatten to (id,count) long-long pairs
    // plus (sum_temp,sum_rainfall) double pairs, gather variable-length
    // arrays via Gatherv, merge on rank 0.
    int local_station_entries = (int)station_map.size();
    int local_interval_entries = (int)interval_map.size();

    vector<long long> station_ids_counts;
    vector<double> station_sums;
    station_ids_counts.reserve(2 * local_station_entries);
    station_sums.reserve(2 * local_station_entries);
    for (const auto &entry : station_map) {
        station_ids_counts.push_back(entry.first);
        station_ids_counts.push_back(entry.second.count);
        station_sums.push_back(entry.second.sum_temp.sum);
        station_sums.push_back(entry.second.sum_rainfall.sum);
    }

    vector<long long> interval_ids_counts;
    interval_ids_counts.reserve(2 * local_interval_entries);
    for (const auto &entry : interval_map) {
        interval_ids_counts.push_back(entry.first);
        interval_ids_counts.push_back(entry.second);
    }

    vector<int> station_counts(size), station_displs(size);
    vector<int> interval_counts(size), interval_displs(size);
    MPI_Gather(&local_station_entries, 1, MPI_INT, station_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_interval_entries, 1, MPI_INT, interval_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    int total_station_entries = 0, total_interval_entries = 0;
    if (rank == 0) {
        int offset = 0;
        for (int p = 0; p < size; ++p) { station_displs[p] = offset; offset += station_counts[p]; }
        total_station_entries = offset;
        offset = 0;
        for (int p = 0; p < size; ++p) { interval_displs[p] = offset; offset += interval_counts[p]; }
        total_interval_entries = offset;
    }

    // Convert entry-counts to element-counts (2 elements per entry) for Gatherv.
    vector<int> station_counts_x2(size), station_displs_x2(size);
    vector<int> interval_counts_x2(size), interval_displs_x2(size);
    if (rank == 0) {
        for (int p = 0; p < size; ++p) {
            station_counts_x2[p] = station_counts[p] * 2;
            station_displs_x2[p] = station_displs[p] * 2;
            interval_counts_x2[p] = interval_counts[p] * 2;
            interval_displs_x2[p] = interval_displs[p] * 2;
        }
    }

    vector<long long> all_station_ids_counts, all_interval_ids_counts;
    vector<double> all_station_sums;
    if (rank == 0) {
        all_station_ids_counts.resize(2 * total_station_entries);
        all_station_sums.resize(2 * total_station_entries);
        all_interval_ids_counts.resize(2 * total_interval_entries);
    }

    MPI_Gatherv(station_ids_counts.data(), 2 * local_station_entries, MPI_LONG_LONG,
                rank == 0 ? all_station_ids_counts.data() : nullptr,
                rank == 0 ? station_counts_x2.data() : nullptr,
                rank == 0 ? station_displs_x2.data() : nullptr,
                MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gatherv(station_sums.data(), 2 * local_station_entries, MPI_DOUBLE,
                rank == 0 ? all_station_sums.data() : nullptr,
                rank == 0 ? station_counts_x2.data() : nullptr,
                rank == 0 ? station_displs_x2.data() : nullptr,
                MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gatherv(interval_ids_counts.data(), 2 * local_interval_entries, MPI_LONG_LONG,
                rank == 0 ? all_interval_ids_counts.data() : nullptr,
                rank == 0 ? interval_counts_x2.data() : nullptr,
                rank == 0 ? interval_displs_x2.data() : nullptr,
                MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    double comm_time = MPI_Wtime() - comm_start;

    if (rank == 0) {
        double merge_start = MPI_Wtime();

        // Merge station entries by station_id.
        unordered_map<int, StationAgg> merged_stations;
        for (int i = 0; i < total_station_entries; ++i) {
            int station_id = (int)all_station_ids_counts[2 * i];
            long long cnt = all_station_ids_counts[2 * i + 1];
            double st_sum_temp = all_station_sums[2 * i];
            double st_sum_rain = all_station_sums[2 * i + 1];
            StationAgg &agg = merged_stations[station_id];
            agg.count += cnt;
            agg.sum_temp.add(st_sum_temp);
            agg.sum_rainfall.add(st_sum_rain);
        }

        // Merge interval entries by interval_id.
        unordered_map<long long, long long> merged_intervals;
        for (int i = 0; i < total_interval_entries; ++i) {
            long long interval_id = all_interval_ids_counts[2 * i];
            long long cnt = all_interval_ids_counts[2 * i + 1];
            merged_intervals[interval_id] += cnt;
        }

        long long busiest_interval = -1, busiest_count = -1;
        for (const auto &entry : merged_intervals) {
            if (entry.second > busiest_count ||
                (entry.second == busiest_count && entry.first < busiest_interval)) {
                busiest_count = entry.second;
                busiest_interval = entry.first;
            }
        }

        vector<pair<int, StationAgg>> stations(merged_stations.begin(), merged_stations.end());
        sort(stations.begin(), stations.end(), [](const auto &a, const auto &b) {
            if (a.second.count != b.second.count) return a.second.count > b.second.count;
            return a.first < b.first;
        });

        // Resolve global hottest/coldest from the P gathered candidates.
        double best_hot_temp = -1e300; long long best_hot_ts = 0; int best_hot_station = 0;
        double best_cold_temp = 1e300; long long best_cold_ts = 0; int best_cold_station = 0;
        for (int p = 0; p < size; ++p) {
            double t = hot_gather[3 * p], ts = hot_gather[3 * p + 1], sid = hot_gather[3 * p + 2];
            if (t > best_hot_temp || (t == best_hot_temp && (long long)ts < best_hot_ts) ||
                (t == best_hot_temp && (long long)ts == best_hot_ts && (int)sid < best_hot_station)) {
                best_hot_temp = t; best_hot_ts = (long long)ts; best_hot_station = (int)sid;
            }
            double ct = cold_gather[3 * p], cts = cold_gather[3 * p + 1], csid = cold_gather[3 * p + 2];
            if (ct < best_cold_temp || (ct == best_cold_temp && (long long)cts < best_cold_ts) ||
                (ct == best_cold_temp && (long long)cts == best_cold_ts && (int)csid < best_cold_station)) {
                best_cold_temp = ct; best_cold_ts = (long long)cts; best_cold_station = (int)csid;
            }
        }

        double avg_temp = sum_reduced[0] / n;
        double avg_humidity = sum_reduced[1] / n;
        double avg_pressure = sum_reduced[2] / n;
        double total_rainfall = sum_reduced[3];
        double avg_wind = sum_reduced[4] / n;
        long long extreme_total = (long long)sum_reduced[5];

        double merge_time = MPI_Wtime() - merge_start;

        cout << fixed << setprecision(2);
        cout << "TOTAL_MEASUREMENTS " << n << "\n";
        cout << "AVERAGE_TEMPERATURE " << avg_temp << "\n";
        cout << "MIN_TEMPERATURE " << min_reduced[0] << "\n";
        cout << "MAX_TEMPERATURE " << max_reduced[0] << "\n";
        cout << "AVERAGE_HUMIDITY " << avg_humidity << "\n";
        cout << "MIN_HUMIDITY " << min_reduced[1] << "\n";
        cout << "MAX_HUMIDITY " << max_reduced[1] << "\n";
        cout << "AVERAGE_PRESSURE " << avg_pressure << "\n";
        cout << "MIN_PRESSURE " << min_reduced[2] << "\n";
        cout << "MAX_PRESSURE " << max_reduced[2] << "\n";
        cout << "TOTAL_RAINFALL " << total_rainfall << "\n";
        cout << "MAX_RAINFALL " << max_reduced[3] << "\n";
        cout << "AVERAGE_WIND_SPEED " << avg_wind << "\n";
        cout << "MAX_WIND_SPEED " << max_reduced[4] << "\n";
        cout << "EXTREME_TEMPERATURE_EVENTS " << extreme_total << "\n";
        cout << "HOTTEST_MEASUREMENT " << best_hot_temp << " " << best_hot_station << " " << best_hot_ts << "\n";
        cout << "COLDEST_MEASUREMENT " << best_cold_temp << " " << best_cold_station << " " << best_cold_ts << "\n";
        cout << "BUSIEST_INTERVAL " << busiest_interval << " " << busiest_count << "\n";
        cout << "TOP_STATIONS\n";
        long long shown = min<long long>(k, (long long)stations.size());
        for (long long i = 0; i < shown; ++i) {
            const auto &st = stations[i];
            double station_avg_temp = st.second.sum_temp.sum / st.second.count;
            cout << st.first << " " << st.second.count << " " << station_avg_temp << " "
                 << st.second.sum_rainfall.sum << "\n";
        }

        double max_setup, max_scatter, max_compute, max_comm;
        MPI_Reduce(&setup_time, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&scatter_time, &max_scatter, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&compute_time, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&comm_time, &max_comm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        double algo_time = max_setup + max_scatter + max_compute + max_comm + merge_time;
        cerr << "MPI_PHASES setup=" << max_setup << " scatter=" << max_scatter
             << " compute=" << max_compute << " comm=" << max_comm
             << " merge=" << merge_time << " algo=" << algo_time << endl;
    } else {
        double dummy;
        MPI_Reduce(&setup_time, &dummy, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&scatter_time, &dummy, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&compute_time, &dummy, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&comm_time, &dummy, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }

    MPI_Type_free(&record_type);
    MPI_Finalize();
    return 0;
}
