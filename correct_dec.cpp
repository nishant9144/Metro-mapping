#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <cmath>

struct Decoder {
    std::string basename;
    int scenario, N, M, K, J, Pcount;
    std::vector<std::pair<int, int>> starts, ends, popular;

    Decoder(const std::string& base) : basename(base) {}

    bool readCity() {
        std::ifstream fin(basename + ".city");
        if (!fin) {
            std::cerr << "Cannot open " << basename << ".city" << std::endl;
            return false;
        }
        fin >> scenario;
        if (!(scenario == 1 || scenario == 2)) {
            std::cerr << "Bad scenario" << std::endl;
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

    bool readSatOutput(std::vector<bool>& assignment, bool& isSat) {
        std::ifstream fin(basename + ".satoutput");
        if (!fin) {
            std::cerr << "Cannot open " << basename << ".satoutput" << std::endl;
            return false;
        }
        std::string first_token;
        if (!(fin >> first_token)) {
            std::cerr << "Empty satoutput file" << std::endl;
            return false;
        }

        std::vector<int> lits;
        if (first_token == "SAT" || first_token == "SATISFIABLE") {
            isSat = true;
        } else if (first_token == "UNSAT" || first_token == "UNSATISFIABLE") {
            isSat = false;
            return true;
        } else {
            isSat = true;
            try {
                lits.push_back(std::stoi(first_token));
            } catch (const std::invalid_argument&) {
                std::cerr << "Invalid satoutput format" << std::endl;
                return false;
            }
        }
        
        int x;
        while (fin >> x) {
            if (x != 0) lits.push_back(x);
        }
        
        int maxvar = 0;
        for (int v : lits) maxvar = std::max(maxvar, std::abs(v));
        assignment.assign(maxvar + 1, false);
        for (int v : lits) {
            if (v > 0 && v < (int)assignment.size()) assignment[v] = true;
        }
        fin.close();
        return true;
    }

    void decode() {
        std::vector<bool> assignment;
        bool isSat = false;
        if (!readSatOutput(assignment, isSat)) { return; }

        std::ofstream fout(basename + ".metromap");
        if (!fout) {
            std::cerr << "Cannot open output metromap file" << std::endl;
            return;
        }

        if (!isSat) {
            fout << "0\n";
            fout.close();
            std::cerr << "UNSAT -> wrote 0" << std::endl;
            return;
        }

        // Reconstruct variable numbering exactly like the encoder
        long long L = (long long)N * M;
        int var_idx = 0;
        std::vector<int> Xid(K * N * M);
        for(size_t i = 0; i < Xid.size(); ++i) Xid[i] = ++var_idx;
        
        long long Ptot = (long long)K * N * M * L;
        std::vector<int> Pid((size_t)Ptot);
        for(size_t i = 0; i < Pid.size(); ++i) Pid[i] = ++var_idx;
        
        std::vector<int> Uid(K*L), Hid(K*L), Zid(K*L);
        for(int k=0; k<K; ++k) for(long long t=0; t<L; ++t) {
            Uid[k*L+t] = ++var_idx; Hid[k*L+t] = ++var_idx; Zid[k*L+t] = ++var_idx;
        }

        auto idP = [&](int k, int x, int y, int t) { long long idx = (((long long)k * N + x) * M + y) * L + t; return Pid.at((size_t)idx); };
        auto idU = [&](int k, int t){ return Uid.at(k*L + t); };
        
        std::vector<std::vector<std::pair<int, int>>> paths(K);
        for (int k = 0; k < K; k++) {
            int t_last = -1;
            for (long long t = 0; t < L; t++) {
                int var = idU(k, (int)t);
                if (var < (int)assignment.size() && assignment[var]) {
                    t_last = (int)t;
                } else {
                    break;
                }
            }

            if (t_last == -1) {
                fout << "0\n";
                continue;
            }

            std::vector<std::pair<int,int>> path_k(t_last + 1);
            for (int t = 0; t <= t_last; t++) {
                bool found_cell = false;
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    int var = idP(k, x, y, t);
                    if (var < (int)assignment.size() && assignment[var]) {
                        path_k[t] = {x, y};
                        found_cell = true;
                        break;
                    }
                }
                if (!found_cell) { t_last = -2; break; }
            }
            if (t_last == -2) continue;
            paths[k] = path_k;
        }

        // Construct direction sequences and write to file
        for (int k = 0; k < K; k++) {
            auto& seq = paths[k];
            if (seq.empty()) continue;

            size_t end_idx = seq.size() - 1;
            for(size_t i = 0; i < seq.size(); ++i) {
                if (seq[i] == ends[k]) {
                    end_idx = i;
                    break;
                }
            }

            for (size_t t = 0; t < end_idx; ++t) {
                auto a = seq[t]; // current cell {x, y}
                auto b = seq[t + 1]; // next cell {x, y}

                // *** CORRECTED LOGIC ***
                if (b.first > a.first) fout << 'R';      // x increases -> Right
                else if (b.first < a.first) fout << 'L'; // x decreases -> Left
                else if (b.second > a.second) fout << 'D';// y increases -> Down
                else if (b.second < a.second) fout << 'U';// y decreases -> Up
            }
            fout << "0\n";
        }
        fout.close();
        std::cerr << "Decoded and wrote " << basename << ".metromap" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <basename>" << std::endl;
        return 1;
    }
    Decoder dec(argv[1]);
    if (!dec.readCity()) return 2;
    dec.decode();
    return 0;
}