#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <unordered_set>
using namespace std;

const int DIRS = 4;
const int DX[4] = {1,0,-1,0};
const int DY[4] = {0,1,0,-1};
const char DIR_LETTER[4] = {'R','D','L','U'};

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
    VarFormula(int K_, int N_, int M_, int J_):K(K_),N(N_),M(M_),J(J_) {
        num_use = 1LL * K * N * M;
        num_V = 1LL * K * N * M * DIRS * (J + 1);
        total_vars = num_use + num_V;
    }
    long long use_index(int k,int x,int y) const { return (1LL * k * N + x) * M + y; }
    long long V_index(int k,int x,int y,int d,int t) const {
        long long base = (1LL * k * N + x) * M + y;
        return ((base * DIRS) + d) * (J + 1) + t;
    }
    long long use_var_id(int k,int x,int y) const { return use_index(k,x,y) + 1; }
    long long V_var_id(int k,int x,int y,int d,int t) const { return num_use + V_index(k,x,y,d,t) + 1; }
    long long num_vars() const { return total_vars; }
};

bool in_grid(int x,int y,int N,int M) { return x>=0 && x<N && y>=0 && y<M; }

pair<bool, unordered_set<int>> parse_minisat(const string &path) {
    ifstream fin(path);
    if (!fin) { cerr << "Error: cannot open minisat output " << path << "\n"; exit(1); }
    vector<string> lines;
    string line;
    while (getline(fin, line)) lines.push_back(line);
    if (lines.empty()) { cerr << "Error: empty minisat output\n"; exit(1); }
    if (lines[0].find("UNSAT") != string::npos || lines[0].find("UNSATISFIABLE") != string::npos) return {false, {}};
    unordered_set<int> trues;
    trues.reserve(65536);
    for (auto &ln : lines) {
        stringstream ss(ln);
        string tok;
        while (ss >> tok) {
            if (tok == "v" || tok == "V") continue;
            try {
                long long v = stoll(tok);
                if (v == 0) continue;
                if (v > 0) trues.insert((int)v);
            } catch(...) {}
        }
    }
    return {true, trues};
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc != 2) { cerr << "Usage: " << argv[0] << " <basename>\n"; return 1; }
    string base = argv[1];
    string cityfile = base + ".city";
    string satfile = base + ".satoutput";
    string outfile = base + ".metromap";

    City city = parse_city(cityfile);
    auto [sat, true_vars] = parse_minisat(satfile);
    ofstream fout(outfile);
    if (!fout) { cerr << "Error: cannot open " << outfile << " for writing\n"; return 1; }
    if (!sat) { fout << "0\n"; cerr << "UNSAT -> wrote 0 to " << outfile << "\n"; return 0; }

    VarFormula vf(city.K, city.N, city.M, city.J);

    // reconstruct each line by walking from start, using formula to compute ids
    for (int k=0;k<city.K;++k) {
        auto [sx,sy] = city.starts[k];
        auto [ex,ey] = city.ends[k];

        vector<char> path;
        bool found_first = false;
        int cur_x=0, cur_y=0, cur_d=0, cur_t=0;

        for (int d=0; d<DIRS; ++d) {
            int nx = sx + DX[d], ny = sy + DY[d];
            if (!in_grid(nx,ny,city.N,city.M)) continue;
            long long vid = vf.V_var_id(k,nx,ny,d,0);
            if (true_vars.find((int)vid) != true_vars.end()) {
                path.push_back(DIR_LETTER[d]);
                cur_x = nx; cur_y = ny; cur_d = d; cur_t = 0;
                found_first = true;
                break;
            }
        }
        if (!found_first) { fout << "0\n"; continue; }

        int steps = 0;
        int max_steps = city.N * city.M + 5;
        bool stuck = false;
        while (!(cur_x == ex && cur_y == ey)) {
            ++steps;
            if (steps > max_steps) { stuck = true; break; }
            bool advanced = false;
            for (int d_move=0; d_move<DIRS; ++d_move) {
                int nx = cur_x + DX[d_move], ny = cur_y + DY[d_move];
                if (!in_grid(nx,ny,city.N,city.M)) continue;
                int tprime = (d_move == cur_d ? cur_t : cur_t + 1);
                if (tprime > city.J) continue;
                long long vid = vf.V_var_id(k,nx,ny,d_move,tprime);
                if (true_vars.find((int)vid) != true_vars.end()) {
                    path.push_back(DIR_LETTER[d_move]);
                    cur_x = nx; cur_y = ny; cur_d = d_move; cur_t = tprime;
                    advanced = true;
                    break;
                }
            }
            if (!advanced) { stuck = true; break; }
        }
        if (stuck) { fout << "0\n"; continue; }
        // write path
        for (size_t i=0;i<path.size(); ++i) {
            if (i) fout << ' ';
            fout << path[i];
        }
        if (!path.empty()) fout << " 0\n"; else fout << "0\n";
    }

    fout.close();
    return 0;
}
