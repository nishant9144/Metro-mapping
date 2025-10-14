#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
using namespace std;

using Clause = vector<long long>;

const int DIRS = 4;
const int DX[4] = {1,0,-1,0};
const int DY[4] = {0,1,0,-1};

struct City {
    int scenario;
    int N, M, K, J;
    vector<pair<int,int>> starts;
    vector<pair<int,int>> ends;
    vector<pair<int,int>> popular;
};

vector<string> tokenize_file(const string &path) {
    ifstream fin(path);
    if (!fin) { cerr << "Error: cannot open " << path << "\n"; exit(1); }
    vector<string> tokens;
    string line;
    while (getline(fin, line)) {
        size_t pos = line.find('#');
        if (pos != string::npos) line.resize(pos);
        stringstream ss(line);
        string tok;
        while (ss >> tok) tokens.push_back(tok);
    }
    return tokens;
}

City parse_city(const string &path) {
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
    long long core_vars;
    VarFormula(int K_, int N_, int M_, int J_): K(K_), N(N_), M(M_), J(J_) {
        num_use = 1LL * K * N * M;
        num_V = 1LL * K * N * M * DIRS * (J + 1);
        core_vars = num_use + num_V;
    }
    long long use_index(int k,int x,int y) const {
        return (1LL * k * N + x) * M + y;
    }
    long long V_index(int k,int x,int y,int d,int t) const {
        long long base = (1LL * k * N + x) * M + y;
        return ((base * DIRS) + d) * (J + 1) + t;
    }
    long long use_var_id(int k,int x,int y) const { return use_index(k,x,y) + 1; }
    long long V_var_id(int k,int x,int y,int d,int t) const { return num_use + V_index(k,x,y,d,t) + 1; }
    long long core_var_count() const { return core_vars; }
};

bool in_grid(int x,int y,int N,int M) {
    return x >= 0 && x < N && y >= 0 && y < M;
}

struct ClauseSink {
    string tmpfile;
    ofstream tmp;
    long long clause_count;
    long long next_aux_id;
    ClauseSink(const string &tmpfile_, long long core_vars) : tmpfile(tmpfile_), clause_count(0), next_aux_id(core_vars + 1) {
        tmp.open(tmpfile);
        if (!tmp) { cerr << "Error: cannot open tmp file " << tmpfile << "\n"; exit(1); }
    }
    ~ClauseSink() {
        if (tmp.is_open()) tmp.close();
    }
    void write_clause(const Clause &cl) {
        for (long long lit : cl) tmp << lit << ' ';
        tmp << "0\n";
        ++clause_count;
    }
    long long alloc_aux() { return next_aux_id++; }
    long long total_aux_allocated() const { return next_aux_id - 1; }
};

void write_clause_stream(ofstream &os, const Clause &cl) {
    for (long long lit : cl) os << lit << ' ';
    os << "0\n";
}

vector<long long> sinz_sequential_at_most_one(const vector<long long> &vars, ClauseSink &sink) {
    vector<long long> aux;
    size_t n = vars.size();
    if (n <= 1) return aux;
    aux.reserve(n-1);
    for (size_t i=0;i+1<n;++i) aux.push_back(sink.alloc_aux());
    {
        Clause c; c.push_back(-vars[0]); c.push_back(aux[0]); sink.write_clause(c);
    }
    for (size_t i = 1; i+1 < n; ++i) {
        Clause c; c.push_back(-vars[i]); c.push_back(aux[i]); sink.write_clause(c);
    }
    for (size_t i = 1; i+1 < n; ++i) {
        Clause c; c.push_back(-aux[i-1]); c.push_back(aux[i]); sink.write_clause(c);
    }
    for (size_t i = 1; i < n; ++i) {
        Clause c; c.push_back(-vars[i]); c.push_back(-aux[i-1]); sink.write_clause(c);
    }
    return aux;
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
    long long core_vars = vf.core_var_count();

    ClauseSink sink(tmpfile, core_vars);

    Clause cl;

    for (int k=0;k<city.K;++k) {
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            long long useid = vf.use_var_id(k,x,y);
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) {
                long long vid = vf.V_var_id(k,x,y,d,t);
                cl.clear(); cl.push_back(-vid); cl.push_back(useid);
                sink.write_clause(cl);
            }
        }
    }

    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            if (x==sx && y==sy) continue;
            cl.clear(); cl.push_back(-vf.use_var_id(k,x,y));
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) cl.push_back(vf.V_var_id(k,x,y,d,t));
            sink.write_clause(cl);
        }
    }

    for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
        vector<long long> uses;
        uses.reserve(city.K);
        for (int k=0;k<city.K;++k) uses.push_back(vf.use_var_id(k,x,y));
        sinz_sequential_at_most_one(uses, sink);
    }

    for (int k=0;k<city.K;++k) {
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            vector<long long> states;
            states.reserve(DIRS * (city.J + 1));
            for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) states.push_back(vf.V_var_id(k,x,y,d,t));
            sinz_sequential_at_most_one(states, sink);
        }
    }

    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        auto [ex,ey] = city.ends[k];
        cl.clear(); cl.push_back(vf.use_var_id(k,sx,sy)); sink.write_clause(cl);
        cl.clear(); cl.push_back(vf.use_var_id(k,ex,ey)); sink.write_clause(cl);
    }

    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        for (int d=0; d<DIRS; ++d) for (int t=0; t<=city.J; ++t) {
            cl.clear(); cl.push_back(-vf.V_var_id(k,sx,sy,d,t)); sink.write_clause(cl);
        }
    }

    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        cl.clear();
        for (int d=0; d<DIRS; ++d) {
            int nx = sx + DX[d], ny = sy + DY[d];
            if (in_grid(nx,ny,city.N,city.M)) cl.push_back(vf.V_var_id(k,nx,ny,d,0));
        }
        if (cl.empty()) {
            cl.clear(); cl.push_back(-vf.use_var_id(k,sx,sy)); sink.write_clause(cl);
        } else sink.write_clause(cl);
    }

    for (int k=0;k<city.K;++k) {
        auto [ex,ey] = city.ends[k];
        for (int d=0; d<DIRS; ++d) {
            int nx = ex + DX[d], ny = ey + DY[d];
            if (!in_grid(nx,ny,city.N,city.M)) continue;
            for (int t=0;t<=city.J;++t) {
                cl.clear(); cl.push_back(-vf.V_var_id(k,nx,ny,d,t)); sink.write_clause(cl);
            }
        }
    }

    for (int k=0;k<city.K;++k) {
        auto [ex,ey] = city.ends[k];
        for (int x=0;x<city.N;++x) for (int y=0;y<city.M;++y) {
            if (x==ex && y==ey) continue;
            for (int d_entry=0; d_entry<DIRS; ++d_entry) for (int t_entry=0; t_entry<=city.J; ++t_entry) {
                long long v_entry = vf.V_var_id(k,x,y,d_entry,t_entry);
                vector<long long> exits;
                for (int d_move=0; d_move<DIRS; ++d_move) {
                    int nx = x + DX[d_move], ny = y + DY[d_move];
                    if (!in_grid(nx,ny,city.N,city.M)) continue;
                    int tprime = (d_move == d_entry ? t_entry : t_entry + 1);
                    if (tprime <= city.J) exits.push_back(vf.V_var_id(k,nx,ny,d_move,tprime));
                }
                if (exits.empty()) {
                    cl.clear(); cl.push_back(-v_entry); sink.write_clause(cl);
                } else {
                    cl.clear(); cl.push_back(-v_entry);
                    for (auto e : exits) cl.push_back(e);
                    sink.write_clause(cl);
                    if (exits.size() <= 6) {
                        for (size_t i=0;i<exits.size();++i)
                            for (size_t j=i+1;j<exits.size();++j) {
                                Clause c; c.push_back(-v_entry); c.push_back(-exits[i]); c.push_back(-exits[j]);
                                sink.write_clause(c);
                            }
                    } else {
                        for (size_t i=0;i<exits.size();++i)
                            for (size_t j=i+1;j<exits.size();++j) {
                                Clause c; c.push_back(-v_entry); c.push_back(-exits[i]); c.push_back(-exits[j]);
                                sink.write_clause(c);
                            }
                    }
                }
            }
        }
    }

    if (city.scenario == 2) {
        for (auto &pp : city.popular) {
            int px = pp.first, py = pp.second;
            cl.clear();
            for (int k=0;k<city.K;++k) cl.push_back(vf.use_var_id(k,px,py));
            sink.write_clause(cl);
        }
    }

    // long long aux_allocated = sink.total_aux_allocated() - core_vars;
    long long last_aux_id = sink.total_aux_allocated();
    long long total_vars = last_aux_id >= (core_vars+1) ? last_aux_id : core_vars;
    long long total_clauses = sink.clause_count;

    ofstream fout(outdimacs);
    if (!fout) { cerr << "Error: cannot open " << outdimacs << " for writing\n"; return 1; }
    fout << "p cnf " << total_vars << " " << total_clauses << "\n";
    sink.tmp.close();
    ifstream tin(tmpfile);
    if (!tin) { cerr << "Error: cannot open " << tmpfile << " for reading\n"; return 1; }
    string line;
    while (getline(tin, line)) fout << line << '\n';
    tin.close();
    fout.close();
    remove(tmpfile.c_str());

    cerr << "Vars: " << total_vars << ", Clauses: " << total_clauses << "\n";
    return 0;
}
