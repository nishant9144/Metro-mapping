#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
using namespace std;
using Clause = vector<int>;

static const int DIRS = 4;
const int DX[4] = {1,0,-1,0};
const int DY[4] = {0,1,0,-1};

struct City {
    int scenario;
    int N, M, K, J;
    vector<pair<int,int>> starts;
    vector<pair<int,int>> ends;
    vector<pair<int,int>> popular;
};

static vector<string> tokenize_file(const string &path) {
    ifstream fin(path);
    if (!fin) { cerr << "Error: cannot open " << path << "\n"; exit(1); }
    vector<string> tokens;
    string line;
    while (getline(fin, line)) {
        // strip comments
        size_t pos = line.find('#');
        if (pos != string::npos) line.resize(pos);
        stringstream ss(line);
        string tok;
        while (ss >> tok) tokens.push_back(tok);
    }
    return tokens;
}

static City parse_city(const string &path) {
    vector<string> tokens = tokenize_file(path);
    if (tokens.empty()) { cerr << "Error: empty or invalid city file\n"; exit(1); }
    size_t idx = 0;
    City city;
    city.scenario = stoi(tokens[idx++]);
    if (city.scenario != 1 && city.scenario != 2) { cerr << "Error: scenario must be 1 or 2\n"; exit(1); }
    if (city.scenario == 1) {
        city.N = stoi(tokens[idx++]); city.M = stoi(tokens[idx++]);
        city.K = stoi(tokens[idx++]); city.J = stoi(tokens[idx++]);
        city.starts.resize(city.K); city.ends.resize(city.K);
        for (int k=0;k<city.K;++k) {
            int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]), ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
            city.starts[k] = {sx,sy}; city.ends[k] = {ex,ey};
        }
    } else {
        city.N = stoi(tokens[idx++]); city.M = stoi(tokens[idx++]);
        city.K = stoi(tokens[idx++]); city.J = stoi(tokens[idx++]);
        int P = stoi(tokens[idx++]);
        city.starts.resize(city.K); city.ends.resize(city.K);
        for (int k=0;k<city.K;++k) {
            int sx = stoi(tokens[idx++]), sy = stoi(tokens[idx++]), ex = stoi(tokens[idx++]), ey = stoi(tokens[idx++]);
            city.starts[k] = {sx,sy}; city.ends[k] = {ex,ey};
        }
        city.popular.clear();
        for (int i=0;i<P;++i) {
            int px = stoi(tokens[idx++]), py = stoi(tokens[idx++]);
            city.popular.emplace_back(px,py);
        }
    }
    return city;
}

struct VarFormula {
    int K,N,M,J;
    long long num_use;
    long long num_V;
    long long total_vars;
    VarFormula(int K_, int N_, int M_, int J_): K(K_), N(N_), M(M_), J(J_) {
        num_use = 1LL * K * N * M;
        num_V = 1LL * K * N * M * DIRS * (J + 1);
        total_vars = num_use + num_V;
    }
    long long use_index(int k,int x,int y) const {
        return (1LL * k * N + x) * M + y; // 0-based index into uses
    }
    long long V_index(int k,int x,int y,int d,int t) const {
        long long base = (1LL * k * N + x) * M + y;
        return ((base * DIRS) + d) * (J + 1) + t; // 0-based index into V block
    }
    int use_var(int k,int x,int y) const = delete; // avoid old-style
    long use_var_id(int k,int x,int y) const {
        return use_index(k,x,y) + 1; // DIMACS ids start at 1
    }
    long long V_var_id(int k,int x,int y,int d,int t) const {
        return num_use + V_index(k,x,y,d,t) + 1;
    }
    long long num_vars() const { return total_vars; }
};

bool in_grid(int x,int y,int N,int M) {
    return x >= 0 && x < N && y >= 0 && y < M;
}

void write_clause_to_stream(ostream &os, const Clause &cl) {
    for (int lit : cl) os << lit << ' ';
    os << "0\n";
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc != 2) { cerr << "Usage: " << argv[0] << " <basename>\n"; return 1; }
    string base = argv[1];
    string cityfile = base + ".city";
    string outdimacs = base + ".satinput";
    string tmpfile = base + ".clauses.tmp";

    City city = parse_city(cityfile);
    VarFormula vf(city.K, city.N, city.M, city.J);

    // stream clauses to temporary file
    ofstream tmp(tmpfile);
    if (!tmp) { cerr << "Error: cannot open temporary clause file " << tmpfile << "\n"; return 1; }
    long long clause_count = 0;

    // small Clause buffer reused to avoid allocations
    Clause cl;

    // 1a: V => use_var  (-V ∨ use)
    for (int k=0;k<city.K;++k) {
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            long long useid = vf.use_var_id(k,x,y);
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) {
                long long vid = vf.V_var_id(k,x,y,d,t);
                cl.clear();
                cl.push_back((int)(-vid));
                cl.push_back((int)useid);
                write_clause_to_stream(tmp, cl);
                ++clause_count;
            }
        }
    }

    // 1b: use_var => OR(V) for non-start cells
    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            if (x==sx && y==sy) continue;
            cl.clear();
            cl.push_back((int)(-vf.use_var_id(k,x,y)));
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) cl.push_back((int)vf.V_var_id(k,x,y,d,t));
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        }
    }

    // 2. Disjoint paths: (-use_k1 ∨ -use_k2)
    for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
        for (int k1=0;k1<city.K;++k1) for (int k2=k1+1;k2<city.K;++k2) {
            cl.clear();
            cl.push_back((int)(-vf.use_var_id(k1,x,y)));
            cl.push_back((int)(-vf.use_var_id(k2,x,y)));
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        }
    }

    // 3. At most one state per cell: pairwise (-V1 ∨ -V2)
    // We will generate pairwise clauses but reuse cl buffer
    for (int k=0;k<city.K;++k) {
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            // collect ids in a small vector on stack
            vector<long long> states;
            states.reserve(DIRS * (city.J + 1));
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) states.push_back(vf.V_var_id(k,x,y,d,t));
            for (size_t i=0;i<states.size();++i) {
                for (size_t j=i+1;j<states.size();++j) {
                    cl.clear();
                    cl.push_back((int)(-states[i]));
                    cl.push_back((int)(-states[j]));
                    write_clause_to_stream(tmp, cl);
                    ++clause_count;
                }
            }
        }
    }

    // 4a. Force start and end cells used (unit clauses)
    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        auto [ex,ey] = city.ends[k];
        cl.clear();
        cl.push_back((int)vf.use_var_id(k,sx,sy));
        write_clause_to_stream(tmp, cl);
        ++clause_count;
        cl.clear();
        cl.push_back((int)vf.use_var_id(k,ex,ey));
        write_clause_to_stream(tmp, cl);
        ++clause_count;
    }

    // 4b. Start cell cannot be entered: (-V[start])
    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        for (int d=0; d<DIRS; ++d) for (int t=0;t<=city.J;++t) {
            cl.clear();
            cl.push_back((int)(-vf.V_var_id(k,sx,sy,d,t)));
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        }
    }

    // 4c. From start at least one neighbor must be entered with t=0
    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        cl.clear();
        for (int d=0; d<DIRS; ++d) {
            int nx = sx + DX[d], ny = sy + DY[d];
            if (in_grid(nx,ny,city.N,city.M)) cl.push_back((int)vf.V_var_id(k,nx,ny,d,0));
        }
        if (cl.empty()) {
            // contradictory clause to force UNSAT relative to forced use
            cl.clear();
            cl.push_back((int)(-vf.use_var_id(k,sx,sy)));
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        } else {
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        }
    }

    // 4x. End cell: forbid moves from end cell
    for (int k=0;k<city.K;++k) {
        auto [ex,ey] = city.ends[k];
        for (int d=0; d<DIRS; ++d) {
            int nx = ex + DX[d], ny = ey + DY[d];
            if (!in_grid(nx,ny,city.N,city.M)) continue;
            for (int t=0;t<=city.J;++t) {
                cl.clear();
                cl.push_back((int)(-vf.V_var_id(k,nx,ny,d,t)));
                write_clause_to_stream(tmp, cl);
                ++clause_count;
            }
        }
    }

    // 4e. Flow conservation for intermediate cells (entry -> exactly one exit)
    for (int k=0;k<city.K;++k) {
        auto [ex,ey] = city.ends[k];
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            if (x==ex && y==ey) continue;
            for (int d_entry=0; d_entry<DIRS; ++d_entry) for (int t_entry=0; t_entry<=city.J; ++t_entry) {
                long long v_entry = vf.V_var_id(k,x,y,d_entry,t_entry);
                // prepare exit list
                vector<int> exits;
                exits.reserve(DIRS);
                for (int d_move=0; d_move<DIRS; ++d_move) {
                    int nx = x + DX[d_move], ny = y + DY[d_move];
                    if (!in_grid(nx,ny,city.N,city.M)) continue;
                    int tprime = (d_move == d_entry ? t_entry : t_entry + 1);
                    if (tprime <= city.J) exits.push_back((int)vf.V_var_id(k,nx,ny,d_move,tprime));
                }
                if (exits.empty()) {
                    cl.clear(); cl.push_back((int)(-v_entry));
                    write_clause_to_stream(tmp, cl);
                    ++clause_count;
                } else {
                    cl.clear();
                    cl.push_back((int)(-v_entry));
                    for (int e : exits) cl.push_back(e);
                    write_clause_to_stream(tmp, cl);
                    ++clause_count;
                    for (size_t i=0;i<exits.size();++i)
                        for (size_t j=i+1;j<exits.size();++j) {
                            cl.clear();
                            cl.push_back((int)(-v_entry));
                            cl.push_back((int)(-exits[i]));
                            cl.push_back((int)(-exits[j]));
                            write_clause_to_stream(tmp, cl);
                            ++clause_count;
                        }
                }
            }
        }
    }

    // Scenario 2 popular cells: require at least one line uses each popular cell
    if (city.scenario == 2) {
        for (auto &pp : city.popular) {
            int px = pp.first, py = pp.second;
            cl.clear();
            for (int k=0;k<city.K;++k) cl.push_back((int)vf.use_var_id(k,px,py));
            write_clause_to_stream(tmp, cl);
            ++clause_count;
        }
    }

    tmp.close();

    // Now write final DIMACS file: header + copy tmpfile
    ofstream fout(outdimacs);
    if (!fout) { cerr << "Error: cannot open " << outdimacs << " for writing\n"; return 1; }
    fout << "p cnf " << vf.num_vars() << " " << clause_count << "\n";
    // stream copy
    ifstream tin(tmpfile);
    if (!tin) { cerr << "Error: cannot re-open temporary clause file\n"; return 1; }
    string line;
    while (getline(tin, line)) {
        fout << line << "\n";
    }
    tin.close();
    fout.close();
    // remove temporary file
    remove(tmpfile.c_str());

    cout << "Vars: " << vf.num_vars() << "\n" << "Clauses: " << clause_count << "\n";
    return 0;
}
