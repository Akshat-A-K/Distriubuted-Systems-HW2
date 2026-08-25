#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <filename>" << endl;
        return 1;
    }

    string filename = argv[1];
    ifstream infile(filename);
    if (!infile) {
        cerr << "Error opening file: " << filename << endl;
        return 1;
    }

    long long n;
    if (!(infile >> n)) {
        cerr << "Error reading N from file." << endl;
        return 1;
    }

    vector<int> data(n);
    for (long long i = 0; i < n; ++i) {
        if (!(infile >> data[i])) {
            cerr << "Error reading data element at index " << i << endl;
            return 1;
        }
    }
    infile.close();

    sort(data.begin(), data.end());

    for (long long i = 0; i < n; ++i) {
        cout << data[i] << (i + 1 == n ? "" : " ");
    }
    cout << "\n";

    return 0;
}
