#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <cstdlib>

class VarManager {
private:
    int N, M, K;
    std::vector<long long> L_k_offsets;
    int var_count;

    int x_base, p_base, u_base, h_base, z_base, rend_base;

public:
    VarManager(int n, int m, int k, const std::vector<long long>& l_values) 
        : N(n), M(m), K(k) {
        
        long long total_L = 0;
        L_k_offsets.push_back(0);
        for(long long l : l_values) {
            total_L += l;
            L_k_offsets.push_back(total_L);
        }

        long long num_x = K * N * M;
        long long num_p = total_L * N * M;
        long long num_u = total_L;
        long long num_h = total_L;
        long long num_z = total_L;
        long long num_rend = total_L;
        
        var_count = 0;
        x_base = newVarBlock(num_x);
        p_base = newVarBlock(num_p);
        u_base = newVarBlock(num_u);
        h_base = newVarBlock(num_h);
        z_base = newVarBlock(num_z);
        rend_base = newVarBlock(num_rend);
    }

    int idX(int k, int x, int y) const { return x_base + k * (N * M) + y * N + x; }
    int idP(int k, int x, int y, int t) const {
        long long p_offset_k = L_k_offsets[k] * N * M;
        return p_base + p_offset_k + t * (N * M) + y * N + x;
    }
    int idU(int k, int t) const { return u_base + L_k_offsets[k] + t; }
    int idH(int k, int t) const { return h_base + L_k_offsets[k] + t; }
    int idZ(int k, int t) const { return z_base + L_k_offsets[k] + t; }
    int idReachedEnd(int k, int t) const { return rend_base + L_k_offsets[k] + t; }

    int newAuxVar() { return ++var_count; }
    int getTotalVars() const { return var_count; }

private:
    int newVarBlock(long long count) {
        if (count == 0) return var_count + 1;
        int base = var_count + 1;
        var_count += count;
        return base;
    }
};


struct Encoder {
    std::string basename;
    int scenario;
    int N, M, K, J, Pcount;
    std::vector<std::pair<int, int>> starts, ends, popular;

    std::vector<std::vector<int>> clauses;
    VarManager* vm = nullptr;

    Encoder(const std::string& base) : basename(base) {}
    ~Encoder() { delete vm; }

    void addClause(const std::vector<int>& c) { clauses.push_back(c); }

    void addAtMostOne(const std::vector<int>& vars) {
        if (vars.size() <= 1) return;
        std::vector<int> s(vars.size() - 1);
        for(size_t i = 0; i < s.size(); ++i) s[i] = vm->newAuxVar();
        addClause({-vars[0], s[0]});
        addClause({-vars[1], -s[0]});
        for (size_t i = 1; i < vars.size() - 1; ++i) {
            addClause({-s[i-1], s[i]});
            addClause({-vars[i], s[i]});
            addClause({s[i-1], vars[i], -s[i]});
            addClause({-vars[i+1], -s[i]});
        }
    }
    
    void addExactlyOne(const std::vector<int>& vars) {
        if (vars.empty()) return;
        addClause(vars);
        addAtMostOne(vars);
    }

    bool readCity() {
        std::ifstream fin(basename + ".city");
        if (!fin) { std::cerr << "Cannot open " << basename << ".city" << std::endl; return false; }
        fin >> scenario;
        if (!(scenario == 1 || scenario == 2)) { std::cerr << "Scenario must be 1 or 2" << std::endl; return false; }
        if (scenario == 1) fin >> N >> M >> K >> J;
        else fin >> N >> M >> K >> J >> Pcount;
        starts.resize(K); ends.resize(K);
        for (int k = 0; k < K; k++) { int sx, sy, ex, ey; fin >> sx >> sy >> ex >> ey; starts[k] = {sx, sy}; ends[k] = {ex, ey}; }
        if (scenario == 2) { popular.resize(Pcount); for (int p = 0; p < Pcount; p++) { int px, py; fin >> px >> py; popular[p] = {px, py}; } }
        fin.close();
        return true;
    }

    void encode() {
        std::vector<long long> L_k_values;
        for (int k = 0; k < K; ++k) {
            int manhattan_dist = std::abs(starts[k].first - ends[k].first) + std::abs(starts[k].second - ends[k].second);
            long long lk = manhattan_dist + 2 * J + 4;
            L_k_values.push_back(std::min((long long)N * M, lk));
        }

        vm = new VarManager(N, M, K, L_k_values);

        for (int k = 0; k < K; ++k) {
            long long L_k = L_k_values[k];
            int ex = ends[k].first;
            int ey = ends[k].second;

            for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                int xid = vm->idX(k, x, y);
                std::vector<int> p_at_cell;
                p_at_cell.reserve(L_k);
                for (long long t = 0; t < L_k; t++) p_at_cell.push_back(vm->idP(k, x, y, (int)t));
                std::vector<int> big_or = {-xid};
                big_or.insert(big_or.end(), p_at_cell.begin(), p_at_cell.end());
                addClause(big_or);
                for (int p_var : p_at_cell) addClause({-p_var, xid});
            }

            addClause({vm->idP(k, starts[k].first, starts[k].second, 0)});

            std::vector<int> reached_end_vars;
            for(long long t=0; t<L_k; ++t) reached_end_vars.push_back(vm->idReachedEnd(k, (int)t));
            addExactlyOne(reached_end_vars);

            for (long long t = 0; t < L_k; ++t) {
                int rend_t = vm->idReachedEnd(k, (int)t);
                
                addClause({-rend_t, vm->idP(k, ex, ey, (int)t)});
                
                if (t < L_k - 1) {
                    addClause({-rend_t, -vm->idU(k, (int)(t + 1))});
                }
            }

            for (long long t = 0; t < L_k; t++) {
                std::vector<int> cellIds;
                cellIds.reserve(N * M);
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) cellIds.push_back(vm->idP(k, x, y, (int)t));
                addAtMostOne(cellIds);
            }
            for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                std::vector<int> timeIds;
                timeIds.reserve(L_k);
                for(long long t=0; t<L_k; ++t) timeIds.push_back(vm->idP(k, x, y, (int)t));
                addAtMostOne(timeIds);
            }
            
            for (long long t = 0; t < L_k; t++) {
                int uid = vm->idU(k, (int)t);
                std::vector<int> p_or = {-uid};
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    addClause({-vm->idP(k, x, y, (int)t), uid});
                    p_or.push_back(vm->idP(k, x, y, (int)t));
                }
                addClause(p_or);

                if (t > 0) {
                    addClause({-vm->idU(k, (int)t), vm->idU(k, (int)(t - 1))});
                    for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                        std::vector<int> neigh;
                        if (x > 0) neigh.push_back(vm->idP(k, x - 1, y, (int)(t - 1)));
                        if (x + 1 < N) neigh.push_back(vm->idP(k, x + 1, y, (int)(t - 1)));
                        if (y > 0) neigh.push_back(vm->idP(k, x, y - 1, (int)(t - 1)));
                        if (y + 1 < M) neigh.push_back(vm->idP(k, x, y + 1, (int)(t - 1)));
                        std::vector<int> cl = {-vm->idP(k, x, y, (int)t)};
                        cl.insert(cl.end(), neigh.begin(), neigh.end());
                        addClause(cl);
                    }
                    int hid = vm->idH(k, (int)t);
                    for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                        int Pcur = vm->idP(k, x, y, (int)t);
                        if (y > 0) addClause({-vm->idP(k, x, y - 1, (int)(t - 1)), -Pcur, -hid});
                        if (y + 1 < M) addClause({-vm->idP(k, x, y + 1, (int)(t - 1)), -Pcur, -hid});
                        if (x > 0) addClause({-vm->idP(k, x - 1, y, (int)(t - 1)), -Pcur, hid});
                        if (x + 1 < N) addClause({-vm->idP(k, x + 1, y, (int)(t - 1)), -Pcur, hid});
                    }
                }
                if (t > 1) {
                    int zid = vm->idZ(k, (int)t);
                    int hid_t = vm->idH(k, (int)t);
                    int hid_t_minus_1 = vm->idH(k, (int)(t-1));
                    addClause({-zid, -hid_t, -hid_t_minus_1});
                    addClause({-zid, hid_t, hid_t_minus_1});
                    addClause({zid, -hid_t, hid_t_minus_1});
                    addClause({zid, hid_t, -hid_t_minus_1});
                }
            }
            std::vector<int> turn_vars;
            for (long long t = 2; t < L_k; t++) turn_vars.push_back(vm->idZ(k, (int)t));
            sinz_at_most_k(turn_vars, std::max(0, J));
        }

        for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
            std::vector<int> lineIds;
            for (int k = 0; k < K; k++) lineIds.push_back(vm->idX(k,x,y));
            addAtMostOne(lineIds);
        }

        if (scenario == 2) {
            for (auto& pp : popular) {
                int px = pp.first, py = pp.second;
                std::vector<int> big_or;
                for (int k = 0; k < K; k++) big_or.push_back(vm->idX(k, px, py));
                addClause(big_or);
            }
        }

        std::ofstream fout(basename + ".satinput");
        fout << "p cnf " << vm->getTotalVars() << " " << clauses.size() << "\n";
        for (auto& c : clauses) {
            for (int lit : c) fout << lit << ' ';
            fout << "0\n";
        }
        fout.close();
        std::cerr << "Wrote " << basename << ".satinput : vars=" << vm->getTotalVars() << " clauses=" << clauses.size() << std::endl;
    }

    void sinz_at_most_k(const std::vector<int>& vs, int k_limit) {
        int n = vs.size();
        if (k_limit < 0) { addClause({}); return; }
        if (n <= k_limit) return;
        std::vector<std::vector<int>> s(n, std::vector<int>(k_limit + 2, 0));
        for(int i = 0; i < n; ++i) for (int j = 1; j <= k_limit + 1; ++j) s[i][j] = vm->newAuxVar();
        addClause({-vs[0], s[0][1]});
        for (int j = 2; j <= k_limit + 1; ++j) addClause({-s[0][j]});
        for (int i = 1; i < n; ++i) {
            addClause({-s[i-1][1], s[i][1]});
            addClause({-vs[i], s[i][1]});
            addClause({s[i-1][1], vs[i], -s[i][1]});
            for (int j = 2; j <= k_limit + 1; ++j) {
                addClause({-s[i-1][j], s[i][j]});
                addClause({-s[i-1][j-1], -vs[i], s[i][j]});
                addClause({s[i-1][j], s[i-1][j-1], -s[i][j]});
                addClause({s[i-1][j], vs[i], -s[i][j]});
            }
        }
        addClause({-s[n - 1][k_limit + 1]});
    }
};

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <basename>" << std::endl; return 1; }
    Encoder enc(argv[1]);
    if (!enc.readCity()) return 2;
    enc.encode();
    return 0;
}