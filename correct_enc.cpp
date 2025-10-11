#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <cstdlib>

struct Encoder {
    std::string basename;
    int scenario;
    int N, M, K, J, Pcount;
    std::vector<std::pair<int, int>> starts, ends, popular;

    // DIMACS building
    std::vector<std::vector<int>> clauses;
    int var_counter = 0;
    int newVar() { return ++var_counter; }
    void addClause(const std::vector<int>& c) { clauses.push_back(c); }

    Encoder(const std::string& base) : basename(base), scenario(0), N(0), M(0), K(0), J(0), Pcount(0) {}

    bool readCity() {
        std::ifstream fin(basename + ".city");
        if (!fin) {
            std::cerr << "Cannot open " << basename << ".city" << std::endl;
            return false;
        }
        fin >> scenario;
        if (!(scenario == 1 || scenario == 2)) {
            std::cerr << "Scenario must be 1 or 2" << std::endl;
            return false;
        }
        if (scenario == 1) {
            fin >> N >> M >> K >> J;
        } else {
            fin >> N >> M >> K >> J >> Pcount;
        }
        starts.resize(K);
        ends.resize(K);
        for (int k = 0; k < K; k++) {
            int sx, sy, ex, ey;
            fin >> sx >> sy >> ex >> ey;
            starts[k] = {sx, sy};
            ends[k] = {ex, ey};
        }
        if (scenario == 2) {
            popular.resize(Pcount);
            for (int p = 0; p < Pcount; p++) {
                int px, py;
                fin >> px >> py;
                popular[p] = {px, py};
            }
        }
        fin.close();
        return true;
    }

    void encode() {
        // Variable Definitions:
        // P(k,x,y,t): Metro line 'k' is at cell (x,y) at step 't'. (Primary variable)
        // X(k,x,y):   Metro line 'k' occupies cell (x,y) at *any* step.
        // U(k,t):     Path 'k' is active (exists) up to at least step 't'.
        // H(k,t):     The move for line 'k' to its position at step 't' was horizontal.
        // Z(k,t):     A turn occurred for line 'k' leading to step 't'.

        long long L = (long long)N * M; // Max path length

        std::vector<int> Xid((size_t)K * N * M);
        for (size_t idx = 0; idx < Xid.size(); ++idx) Xid[idx] = newVar();

        long long Ptot = (long long)K * N * M * L;
        if (Ptot > 200000000) {
            std::cerr << "Problem too large for position variables. Aborting." << std::endl;
            exit(1);
        }
        std::vector<int> Pid((size_t)Ptot);
        for (long long z = 0; z < Ptot; ++z) Pid[(size_t)z] = newVar();
        
        // Lambda functions for consistent variable indexing
        auto idX = [&](int k, int x, int y) { return Xid.at((k * N + x) * M + y); };
        auto idP = [&](int k, int x, int y, int t) {
            long long index = (((long long)k * N + x) * M + y) * L + t;
            return Pid.at((size_t)index);
        };

        std::vector<int> Uid((size_t)K * L), Hid((size_t)K * L), Zid((size_t)K * L);
        for (int k = 0; k < K; k++) {
            for (long long t = 0; t < L; t++) {
                Uid[k * L + t] = newVar();
                Hid[k * L + t] = newVar();
                Zid[k * L + t] = newVar();
            }
        }
        auto idU = [&](int k, int t) { return Uid.at(k * L + t); };
        auto idH = [&](int k, int t) { return Hid.at(k * L + t); };
        auto idZ = [&](int k, int t) { return Zid.at(k * L + t); };

        // --- CONSTRAINTS ---

        // 1) Link occupation vars (X) with step-position vars (P)
        for (int k = 0; k < K; k++) {
            for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                int xid = idX(k, x, y);
                std::vector<int> p_at_cell;
                p_at_cell.reserve(L);
                for (long long t = 0; t < L; t++) {
                    addClause({-idP(k, x, y, (int)t), xid});
                    p_at_cell.push_back(idP(k, x, y, (int)t));
                }
                std::vector<int> big_or = {-xid};
                big_or.insert(big_or.end(), p_at_cell.begin(), p_at_cell.end());
                addClause(big_or);
            }
        }

        // 2) Each path must start at its specified start cell at step t=0.
        for (int k = 0; k < K; k++) {
            addClause({idP(k, starts[k].first, starts[k].second, 0)});
        }

        // 3) Each path must eventually reach its end cell.
        for (int k = 0; k < K; k++) {
            std::vector<int> big_or;
            for (long long t = 0; t < L; t++) {
                big_or.push_back(idP(k, ends[k].first, ends[k].second, (int)t));
            }
            addClause(big_or);
        }

        // 4) For each line at each step, it can only be in at most one cell.
        for (int k = 0; k < K; k++) {
            for (long long t = 0; t < L; t++) {
                std::vector<int> cellIds;
                cellIds.reserve(N * M);
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    cellIds.push_back(idP(k, x, y, (int)t));
                }
                for (size_t a = 0; a < cellIds.size(); ++a) {
                    for (size_t b = a + 1; b < cellIds.size(); ++b) {
                        addClause({-cellIds[a], -cellIds[b]});
                    }
                }
            }
        }

        // 5) Each cell can be visited at most once by any given line (path is simple).
        for (int k = 0; k < K; k++) {
            for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                for (long long t1 = 0; t1 < L; t1++) {
                    for (long long t2 = t1 + 1; t2 < L; t2++) {
                        addClause({-idP(k, x, y, (int)t1), -idP(k, x, y, (int)t2)});
                    }
                }
            }
        }
        
        // 6) Path Continuity
        for (int k = 0; k < K; k++) {
            for (long long t = 1; t < L; t++) {
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    std::vector<int> neigh;
                    if (x > 0) neigh.push_back(idP(k, x - 1, y, (int)(t - 1)));
                    if (x + 1 < N) neigh.push_back(idP(k, x + 1, y, (int)(t - 1)));
                    if (y > 0) neigh.push_back(idP(k, x, y - 1, (int)(t - 1)));
                    if (y + 1 < M) neigh.push_back(idP(k, x, y + 1, (int)(t - 1)));
                    
                    std::vector<int> cl = {-idP(k, x, y, (int)t)};
                    cl.insert(cl.end(), neigh.begin(), neigh.end());
                    addClause(cl);
                }
            }
        }

        // 7) Define U(k,t) as path existence and enforce its monotonicity.
        for (int k = 0; k < K; k++) {
            for (long long t = 0; t < L; t++) {
                int uid = idU(k, (int)t);
                std::vector<int> big_or = {-uid};
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    addClause({-idP(k, x, y, (int)t), uid});
                    big_or.push_back(idP(k, x, y, (int)t));
                }
                addClause(big_or);
                if (t > 0) addClause({-uid, idU(k, (int)(t - 1))});
            }
        }

        // 8) Define H(k,t) to be true if the move from t-1 to t was horizontal.
        for (int k = 0; k < K; k++) {
            for (long long t = 1; t < L; t++) {
                int hid = idH(k, (int)t);
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    int Pcur = idP(k, x, y, (int)t);
                    if (y > 0) addClause({-idP(k, x, y - 1, (int)(t - 1)), -Pcur, -hid}); // Vertical
                    if (y + 1 < M) addClause({-idP(k, x, y + 1, (int)(t - 1)), -Pcur, -hid}); // Vertical
                    if (x > 0) addClause({-idP(k, x - 1, y, (int)(t - 1)), -Pcur, hid}); // Horizontal
                    if (x + 1 < N) addClause({-idP(k, x + 1, y, (int)(t - 1)), -Pcur, hid}); // Horizontal
                }
            }
        }

        // 9) Define Z(k,t) to be true if a turn occurred from t-2 -> t-1 -> t.
        for (int k = 0; k < K; k++) {
            for (long long t = 2; t < L; t++) {
                int zid = idZ(k, (int)t);
                addClause({-zid, idH(k, (int)t), idH(k, (int)(t - 1))});
                addClause({-zid, -idH(k, (int)t), -idH(k, (int)(t - 1))});
            }
        }

        // 10) Enforce mutual exclusion: at most one line per cell.
        for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
            for (int k1 = 0; k1 < K; k1++) for (int k2 = k1 + 1; k2 < K; k2++) {
                addClause({-idX(k1, x, y), -idX(k2, x, y)});
            }
        }

        // 11) For Scenario 2, each popular cell must be visited by at least one line.
        if (scenario == 2) {
            for (auto& pp : popular) {
                int px = pp.first, py = pp.second;
                std::vector<int> big_or;
                for (int k = 0; k < K; k++) {
                    big_or.push_back(idX(k, px, py));
                }
                addClause(big_or);
            }
        }

        // 12) Enforce the turn limit.
        for (int k = 0; k < K; k++) {
            std::vector<int> zs;
            for (long long t = 2; t < L; t++) {
                zs.push_back(idZ(k, (int)t));
            }
            sinz_at_most_k(zs, std::max(0, J));
        }

        // Output DIMACS file
        std::ofstream fout(basename + ".satinput");
        if (!fout) {
            std::cerr << "Cannot open output file" << std::endl;
            exit(1);
        }
        fout << "p cnf " << var_counter << " " << clauses.size() << "\n";
        for (auto& c : clauses) {
            for (int lit : c) fout << lit << ' ';
            fout << "0\n";
        }
        fout.close();
        std::cerr << "Wrote " << basename << ".satinput : vars=" << var_counter << " clauses=" << clauses.size() << std::endl;
    }

    void sinz_at_most_k(const std::vector<int>& vs, int k_limit) {
        int n = (int)vs.size();
        if (n == 0 || k_limit >= n) return;
        
        std::vector<std::vector<int>> s(n, std::vector<int>(k_limit + 1, 0));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < k_limit; j++) {
                s[i][j] = newVar();
            }
        }

        addClause({-vs[0], s[0][0]});
        for (int j = 1; j < k_limit; j++) addClause({-s[0][j]});
        
        for (int i = 1; i < n; i++) {
            addClause({-s[i - 1][0], s[i][0]});
            addClause({-vs[i], s[i][0]});
            addClause({s[i - 1][0], vs[i], -s[i][0]});

            for (int j = 1; j < k_limit; j++) {
                addClause({-s[i-1][j], s[i][j]});
                addClause({-vs[i], -s[i-1][j-1], s[i][j]});
                addClause({s[i-1][j], vs[i], -s[i][j]});
                addClause({s[i-1][j], s[i-1][j-1], -s[i][j]});
            }
        }
        addClause({-vs[n - 1], -s[n - 1][k_limit - 1]});
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <basename>" << std::endl;
        return 1;
    }
    Encoder enc(argv[1]);
    if (!enc.readCity()) return 2;
    enc.encode();
    return 0;
}