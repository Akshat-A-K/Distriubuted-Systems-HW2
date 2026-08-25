#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace std;

static inline bool is_power_of_two(long long n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (argc < 2 || argc > 3) {
        cerr << "Usage: " << argv[0] << " <N> [seed]" << endl;
        return 1;
    }

    long long n = stoll(argv[1]);
    if (!is_power_of_two(n)) {
        cerr << "Error: N (" << n << ") must be a power of 2." << endl;
        return 1;
    }

    unsigned long long seed = (argc >= 3) ? stoull(argv[2]) : 42ULL;
    mt19937_64 rng(seed);
    uniform_int_distribution<int> dist(-1000000, 1000000);

    cout << n << "\n";
    for (long long i = 0; i < n; ++i) {
        cout << dist(rng) << (i + 1 == n ? "\n" : " ");
    }

    return 0;
}
