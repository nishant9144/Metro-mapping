// decoder.cpp
// Usage: ./decoder <basename>
// Reads <basename>.city and <basename>.satoutput and writes <basename>.metromap
// Uses array-based VarManager to reproduce encoder variable IDs.

#include <bits/stdc++.h>
using namespace std;

using VI = vector<int>;

static const int DIRS = 4;
const int DX[4] = {1, 0, -1, 0};
const int DY[4] = {0, 1, 0, -1};
const char DIR_LETTER[4] = {'R','D','L','U'};

struct City {
    int scenario;
    int N, M, K, J;
    vector<pair<int,int>> starts;
    vector<pair<int,int>> ends;
    vector<pair<int,int>> popular;
};

class CityParser {
public:
    static City parse(const string &path) {
        ifstream fin(path);
        if (!fin) { cerr << "Error: cannot open file " << path << "\n"; exit(1); }
        vector<string> lines;
        string line;
        while (getline(fin, line)) {
            string t = line;
            size_t pos = t.find('#');
            if (pos != string::npos) t = t.substr(0, pos);
            auto left = find_if_not(t.begin(), t.end(), ::isspace);
            if (left == t.end()) continue;
            auto right = find_if_not(t.rbegin(), t.rend(), ::isspace).base();
            string trimmed(left, right);
            if (!trimmed.empty()) lines.push_back(trimmed);
        }
        if (lines.empty()) { cerr << "Empty city file\n"; exit(1); }
        vector<string> tokens;
        for (auto &l : lines) {
            stringstream ss(l);
            string tok;
            while (ss >> tok) tokens.push_back(tok);
        }
        size_t idx = 0;
        City city;
        city.scenario = stoi(tokens[idx++]);
        if (city.scenario == 1) {
            city.N = stoi(tokens[idx++]); city.M = stoi(tokens[idx++]); city.K = stoi(tokens[idx++]); city.J = stoi(tokens[idx++]);
            city.starts.resize(city.K); city.ends.resize(city.K);
            for (int k = 0; k < city.K; ++k) {
                int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]), ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
                city.starts[k] = {sx, sy}; city.ends[k] = {ex, ey};
            }
            city.popular.clear();
        } else {
            city.N = stoi(tokens[idx++]); city.M = stoi(tokens[idx++]); city.K = stoi(tokens[idx++]); city.J = stoi(tokens[idx++]);
            int P = stoi(tokens[idx++]);
            city.starts.resize(city.K); city.ends.resize(city.K);
            for (int k = 0; k < city.K; ++k) {
                int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]), ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
                city.starts[k] = {sx, sy}; city.ends[k] = {ex, ey};
            }
            city.popular.clear();
            for (int i = 0; i < P; ++i) {
                int px = stoi(tokens[idx++]), py = stoi(tokens[idx++]);
                city.popular.emplace_back(px, py);
            }
        }
        return city;
    }
};

// same VarManagerArray as in encoder (reproduce IDs)
class VarManagerArray {
    int K, N, M, J;
    int num_use;
    int num_V;
    int total_vars;
    vector<int> use_ids;
    vector<int> V_ids;
public:
    VarManagerArray(int K_, int N_, int M_, int J_)
      : K(K_), N(N_), M(M_), J(J_) {
        num_use = K * N * M;
        num_V = K * N * M * DIRS * (J + 1);
        total_vars = num_use + num_V;
        use_ids.assign(num_use, 0);
        V_ids.assign(num_V, 0);
        int id = 1;
        for (int k = 0; k < K; ++k) for (int x = 0; x < N; ++x) for (int y = 0; y < M; ++y)
            use_ids[use_index(k,x,y)] = id++;
        for (int k = 0; k < K; ++k)
            for (int x = 0; x < N; ++x)
                for (int y = 0; y < M; ++y)
                    for (int d = 0; d < DIRS; ++d)
                        for (int t = 0; t <= J; ++t)
                            V_ids[V_index(k,x,y,d,t)] = id++;
        if (id != total_vars + 1) { cerr << "VarManagerArray ID assignment error\n"; exit(1); }
    }

    inline int use_index(int k, int x, int y) const { return (k * N + x) * M + y; }
    inline int V_index(int k, int x, int y, int d, int t) const { int base = (k * N + x) * M + y; return ((base * DIRS) + d) * (J + 1) + t; }

    int use_var(int k, int x, int y) const { return use_ids[use_index(k,x,y)]; }
    int V_var(int k, int x, int y, int d, int t) const { return V_ids[V_index(k,x,y,d,t)]; }
    int num_vars() const { return total_vars; }
};

pair<bool, unordered_set<int>> parse_minisat(const string &path) {
    ifstream fin(path);
    if (!fin) { cerr << "Error: cannot open minisat output " << path << "\n"; exit(1); }
    vector<string> lines; string l;
    while (getline(fin, l)) lines.push_back(l);
    if (lines.empty()) { cerr << "Error: empty minisat output\n"; exit(1); }
    if (lines[0].find("UNSAT") != string::npos || lines[0].find("UNSATISFIABLE") != string::npos) return {false, {}};
    unordered_set<int> trues;
    for (auto &line : lines) {
        stringstream ss(line); string tok;
        while (ss >> tok) {
            if (tok == "v" || tok == "V") continue;
            try {
                long long val = stoll(tok);
                if (val == 0) continue;
                if (val > 0) trues.insert((int)val);
            } catch (...) {}
        }
    }
    return {true, trues};
}

static inline bool in_grid(int x, int y, int N, int M) { return x >= 0 && x < N && y >= 0 && y < M; }

int main(int argc, char** argv) {
    if (argc != 2) { cerr << "Usage: " << argv[0] << " <basename>\n"; return 1; }
    string base = argv[1];
    string cityfile = base + ".city";
    string satfile = base + ".satoutput";
    string outfile = base + ".metromap";

    City city = CityParser::parse(cityfile);
    auto [sat, true_vars] = parse_minisat(satfile);

    ofstream fout(outfile);
    if (!fout) { cerr << "Error: cannot open " << outfile << " for writing\n"; return 1; }

    if (!sat) {
        fout << "0\n";
        cerr << "Decoder: UNSAT -> wrote 0 to " << outfile << "\n";
        return 0;
    }

    VarManagerArray vm(city.K, city.N, city.M, city.J);

    auto V_true = [&](int k,int x,int y,int d,int t)->bool {
        int vid = vm.V_var(k,x,y,d,t);
        return true_vars.find(vid) != true_vars.end();
    };

    for (int k = 0; k < city.K; ++k) {
        auto [sx, sy] = city.starts[k];
        auto [ex, ey] = city.ends[k];

        vector<char> path;
        bool found_first = false;
        int cur_x = 0, cur_y = 0, cur_d = 0, cur_t = 0;

        for (int d = 0; d < DIRS; ++d) {
            int nx = sx + DX[d], ny = sy + DY[d];
            if (!in_grid(nx, ny, city.N, city.M)) continue;
            if (V_true(k, nx, ny, d, 0)) {
                path.push_back(DIR_LETTER[d]);
                cur_x = nx; cur_y = ny; cur_d = d; cur_t = 0;
                found_first = true;
                break;
            }
        }
        if (!found_first) { fout << "0\n"; continue; }

        set<pair<int,int>> visited;
        visited.insert({sx, sy});
        visited.insert({cur_x, cur_y});
        int steps = 0, max_steps = city.N * city.M + 5;
        bool stuck = false;
        while (!(cur_x == ex && cur_y == ey)) {
            ++steps;
            if (steps > max_steps) { stuck = true; break; }
            bool advanced = false;
            for (int d_move = 0; d_move < DIRS; ++d_move) {
                int nx = cur_x + DX[d_move], ny = cur_y + DY[d_move];
                if (!in_grid(nx, ny, city.N, city.M)) continue;
                int tprime = (d_move == cur_d ? cur_t : cur_t + 1);
                if (tprime > city.J) continue;
                if (V_true(k, nx, ny, d_move, tprime)) {
                    path.push_back(DIR_LETTER[d_move]);
                    cur_x = nx; cur_y = ny; cur_d = d_move; cur_t = tprime;
                    advanced = true;
                    visited.insert({cur_x, cur_y});
                    break;
                }
            }
            if (!advanced) { stuck = true; break; }
        }
        if (stuck) { fout << "0\n"; continue; }
        for (size_t i = 0; i < path.size(); ++i) {
            if (i) fout << ' ';
            fout << path[i];
        }
        if (!path.empty()) fout << " 0\n"; else fout << "0\n";
    }

    fout.close();
    cerr << "Decoder: wrote " << outfile << "\n";
    return 0;
}
