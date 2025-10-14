// encoder.cpp
// Usage: ./encoder <basename>
// Reads <basename>.city and writes <basename>.satinput (DIMACS CNF).
// Uses an array-based VarManager for fast deterministic variable ID assignment.

#include <bits/stdc++.h>
using namespace std;

using Clause = vector<int>;

static const int DIRS = 4;
const int DX[4] = {1, 0, -1, 0};
const int DY[4] = {0, 1, 0, -1};

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
        if (lines.empty()) { cerr << "Error: empty or invalid city file\n"; exit(1); }
        vector<string> tokens;
        for (auto &l : lines) {
            stringstream ss(l);
            string tok;
            while (ss >> tok) tokens.push_back(tok);
        }
        size_t idx = 0;
        City city;
        city.scenario = stoi(tokens[idx++]);
        if (city.scenario != 1 && city.scenario != 2) { cerr << "Error: scenario must be 1 or 2\n"; exit(1); }
        if (city.scenario == 1) {
            city.N = stoi(tokens[idx++]);
            city.M = stoi(tokens[idx++]);
            city.K = stoi(tokens[idx++]);
            city.J = stoi(tokens[idx++]);
            city.starts.resize(city.K);
            city.ends.resize(city.K);
            for (int k = 0; k < city.K; ++k) {
                int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]);
                int ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
                city.starts[k] = {sx, sy};
                city.ends[k] = {ex, ey};
            }
            city.popular.clear();
        } else {
            city.N = stoi(tokens[idx++]);
            city.M = stoi(tokens[idx++]);
            city.K = stoi(tokens[idx++]);
            city.J = stoi(tokens[idx++]);
            int P = stoi(tokens[idx++]);
            city.starts.resize(city.K);
            city.ends.resize(city.K);
            for (int k = 0; k < city.K; ++k) {
                int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]);
                int ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
                city.starts[k] = {sx, sy};
                city.ends[k] = {ex, ey};
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

// Array-based VarManager: deterministic id assignment.
// Ordering: for k=0..K-1, x=0..N-1, y=0..M-1:
//   first all use_var(k,x,y) in that order, then all V(k,x,y,d,t) with d inner then t inner.
class VarManagerArray {
    int K, N, M, J;
    int num_use;
    int num_V;
    int total_vars;
    vector<int> use_ids; // size num_use
    vector<int> V_ids;   // size num_V
public:
    VarManagerArray(int K_, int N_, int M_, int J_)
      : K(K_), N(N_), M(M_), J(J_) {
        num_use = K * N * M;
        num_V = K * N * M * DIRS * (J + 1);
        total_vars = num_use + num_V;
        use_ids.assign(num_use, 0);
        V_ids.assign(num_V, 0);
        int id = 1;
        // assign use ids
        for (int k = 0; k < K; ++k) {
            for (int x = 0; x < N; ++x) {
                for (int y = 0; y < M; ++y) {
                    int idx = use_index(k, x, y);
                    use_ids[idx] = id++;
                }
            }
        }
        // assign V ids
        for (int k = 0; k < K; ++k) {
            for (int x = 0; x < N; ++x) {
                for (int y = 0; y < M; ++y) {
                    for (int d = 0; d < DIRS; ++d) {
                        for (int t = 0; t <= J; ++t) {
                            int idx = V_index(k, x, y, d, t);
                            V_ids[idx] = id++;
                        }
                    }
                }
            }
        }
        // Sanity: id should be total_vars + 1
        if (id != total_vars + 1) {
            cerr << "VarManagerArray internal error: id mismatch\n";
            exit(1);
        }
    }

    inline int use_index(int k, int x, int y) const {
        // (k * N + x) * M + y
        return (k * N + x) * M + y;
    }
    inline int V_index(int k, int x, int y, int d, int t) const {
        // ((((k * N + x) * M + y) * DIRS) + d) * (J+1) + t
        int base = (k * N + x) * M + y;
        return ((base * DIRS) + d) * (J + 1) + t;
    }

    int use_var(int k, int x, int y) const {
        return use_ids[use_index(k, x, y)];
    }
    int V_var(int k, int x, int y, int d, int t) const {
        return V_ids[V_index(k, x, y, d, t)];
    }
    int num_vars() const { return total_vars; }
};

class Encoder {
    City city;
    VarManagerArray vm;
    vector<Clause> clauses;
public:
    Encoder(const City &c) : city(c), vm(c.K, c.N, c.M, c.J) {
        clauses.reserve(1000000);
    }

    static inline bool in_grid(int x, int y, int N, int M) {
        return x >= 0 && x < N && y >= 0 && y < M;
    }

    void add_clause(const Clause &c) { clauses.push_back(c); }

    void encode_all() {
        // 1a: V => use_var  (-V ∨ use)
        for (int k = 0; k < city.K; ++k)
            for (int x = 0; x < city.N; ++x)
                for (int y = 0; y < city.M; ++y)
                    for (int d = 0; d < DIRS; ++d)
                        for (int t = 0; t <= city.J; ++t)
                            add_clause({ -vm.V_var(k,x,y,d,t), vm.use_var(k,x,y) });

        // 1b: use_var => OR(V) for non-start cells
        for (int k = 0; k < city.K; ++k) {
            auto [sx, sy] = city.starts[k];
            for (int x = 0; x < city.N; ++x)
                for (int y = 0; y < city.M; ++y) {
                    if (x == sx && y == sy) continue;
                    Clause cl; cl.push_back(-vm.use_var(k,x,y));
                    for (int d = 0; d < DIRS; ++d) for (int t = 0; t <= city.J; ++t) cl.push_back(vm.V_var(k,x,y,d,t));
                    add_clause(cl);
                }
        }

        // 2: disjoint paths
        for (int x = 0; x < city.N; ++x)
            for (int y = 0; y < city.M; ++y)
                for (int k1 = 0; k1 < city.K; ++k1)
                    for (int k2 = k1 + 1; k2 < city.K; ++k2)
                        add_clause({ -vm.use_var(k1,x,y), -vm.use_var(k2,x,y) });

        // 3: at most one state per cell (pairwise)
        for (int k = 0; k < city.K; ++k)
            for (int x = 0; x < city.N; ++x)
                for (int y = 0; y < city.M; ++y) {
                    vector<int> states;
                    states.reserve(DIRS * (city.J + 1));
                    for (int d = 0; d < DIRS; ++d) for (int t = 0; t <= city.J; ++t) states.push_back(vm.V_var(k,x,y,d,t));
                    for (size_t i = 0; i < states.size(); ++i)
                        for (size_t j = i + 1; j < states.size(); ++j)
                            add_clause({ -states[i], -states[j] });
                }

        // 4a: force start and end cells used
        for (int k = 0; k < city.K; ++k) {
            auto [sx, sy] = city.starts[k];
            auto [ex, ey] = city.ends[k];
            add_clause({ vm.use_var(k,sx,sy) });
            add_clause({ vm.use_var(k,ex,ey) });
        }

        // 4b: start cannot be entered
        for (int k = 0; k < city.K; ++k) {
            auto [sx, sy] = city.starts[k];
            for (int d = 0; d < DIRS; ++d) for (int t = 0; t <= city.J; ++t)
                add_clause({ -vm.V_var(k,sx,sy,d,t) });
        }

        // 4c: from start, at least one neighbor entered with t=0
        for (int k = 0; k < city.K; ++k) {
            auto [sx, sy] = city.starts[k];
            Clause cl;
            for (int d = 0; d < DIRS; ++d) {
                int nx = sx + DX[d], ny = sy + DY[d];
                if (in_grid(nx, ny, city.N, city.M)) cl.push_back(vm.V_var(k, nx, ny, d, 0));
            }
            if (cl.empty()) {
                add_clause({ -vm.use_var(k,sx,sy) }); // forces UNSAT relative to forced use
            } else add_clause(cl);
        }

        // 4x: end cell: forbid moves out of end
        for (int k = 0; k < city.K; ++k) {
            auto [ex, ey] = city.ends[k];
            for (int d = 0; d < DIRS; ++d) {
                int nx = ex + DX[d], ny = ey + DY[d];
                if (!in_grid(nx, ny, city.N, city.M)) continue;
                for (int t = 0; t <= city.J; ++t)
                    add_clause({ -vm.V_var(k, nx, ny, d, t) });
            }
        }

        // 4e: flow conservation for intermediate cells
        for (int k = 0; k < city.K; ++k) {
            auto [ex, ey] = city.ends[k];
            for (int x = 0; x < city.N; ++x)
                for (int y = 0; y < city.M; ++y) {
                    if (x == ex && y == ey) continue;
                    for (int d_entry = 0; d_entry < DIRS; ++d_entry)
                        for (int t_entry = 0; t_entry <= city.J; ++t_entry) {
                            int v_entry = vm.V_var(k,x,y,d_entry,t_entry);
                            vector<int> exits;
                            for (int d_move = 0; d_move < DIRS; ++d_move) {
                                int nx = x + DX[d_move], ny = y + DY[d_move];
                                if (!in_grid(nx, ny, city.N, city.M)) continue;
                                int tprime = (d_move == d_entry ? t_entry : t_entry + 1);
                                if (tprime <= city.J) exits.push_back(vm.V_var(k, nx, ny, d_move, tprime));
                            }
                            if (exits.empty()) add_clause({ -v_entry });
                            else {
                                Clause cl; cl.push_back(-v_entry);
                                for (int e : exits) cl.push_back(e);
                                add_clause(cl);
                                for (size_t i = 0; i < exits.size(); ++i)
                                    for (size_t j = i + 1; j < exits.size(); ++j)
                                        add_clause({ -v_entry, -exits[i], -exits[j] });
                            }
                        }
                }
        }

        // Scenario 2: popular cell constraints
        if (city.scenario == 2) {
            for (auto &pp : city.popular) {
                int px = pp.first, py = pp.second;
                Clause cl;
                for (int k = 0; k < city.K; ++k) cl.push_back(vm.use_var(k,px,py));
                add_clause(cl);
            }
        }
    }

    void write_dimacs(const string &outpath) const {
        ofstream fout(outpath);
        if (!fout) { cerr << "Error: cannot open " << outpath << " for writing\n"; exit(1); }
        fout << "p cnf " << vm.num_vars() << " " << clauses.size() << "\n";
        for (auto &cl : clauses) {
            if (cl.empty()) { fout << "0\n"; continue; }
            for (int lit : cl) fout << lit << " ";
            fout << "0\n";
        }
        fout.close();
    }

    void run_and_write(const string &outpath) {
        encode_all();
        write_dimacs(outpath);
        cerr << "Encoder: wrote " << vm.num_vars() << " vars and " << clauses.size() << " clauses to " << outpath << "\n";
    }
};

int main(int argc, char** argv) {
    if (argc != 2) { cerr << "Usage: " << argv[0] << " <basename>\n"; return 1; }
    string base = argv[1];
    string inpath = base + ".city";
    string outpath = base + ".satinput";
    City city = CityParser::parse(inpath);
    Encoder enc(city);
    enc.run_and_write(outpath);
    return 0;
}
