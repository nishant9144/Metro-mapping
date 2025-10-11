#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <cmath>

// The VarManager MUST be identical to the one in the encoder
// to ensure the variable numbering matches perfectly.
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
    int idReachedEnd(int k, int t) const { return rend_base + L_k_offsets[k] + t; }

private:
    int newVarBlock(long long count) {
        if (count == 0) return var_count + 1;
        int base = var_count + 1;
        var_count += count;
        return base;
    }
};


struct Decoder {
    std::string basename;
    int scenario, N, M, K, J, Pcount;
    std::vector<std::pair<int, int>> starts, ends, popular;

    Decoder(const std::string& base) : basename(base) {}

    bool readCity() {
        std::ifstream fin(basename + ".city");
        if (!fin) { std::cerr << "Cannot open " << basename << ".city" << std::endl; return false; }
        fin >> scenario;
        if (!(scenario == 1 || scenario == 2)) { std::cerr << "Bad scenario" << std::endl; return false; }
        if (scenario == 1) fin >> N >> M >> K >> J;
        else fin >> N >> M >> K >> J >> Pcount;
        starts.resize(K); ends.resize(K);
        for (int k = 0; k < K; k++) { int sx, sy, ex, ey; fin >> sx >> sy >> ex >> ey; starts[k] = {sx, sy}; ends[k] = {ex, ey}; }
        if (scenario == 2) { popular.resize(Pcount); for (int p = 0; p < Pcount; p++) { int px, py; fin >> px >> py; popular[p] = {px, py}; } }
        fin.close(); return true;
    }

    bool readSatOutput(std::vector<bool>& assignment, bool& isSat) {
        std::ifstream fin(basename + ".satoutput");
        if (!fin) { std::cerr << "Cannot open " << basename << ".satoutput" << std::endl; return false; }
        std::string first_token;
        if (!(fin >> first_token)) { std::cerr << "Empty satoutput file" << std::endl; return false; }
        std::vector<int> lits;
        if (first_token == "SAT" || first_token == "SATISFIABLE") isSat = true;
        else if (first_token == "UNSAT" || first_token == "UNSATISFIABLE") { isSat = false; return true; }
        else {
            isSat = true;
            try { lits.push_back(std::stoi(first_token)); }
            catch (const std::invalid_argument&) { std::cerr << "Invalid satoutput format" << std::endl; return false; }
        }
        int x;
        while (fin >> x) if (x != 0) lits.push_back(x);
        int maxvar = 0;
        for (int v : lits) maxvar = std::max(maxvar, std::abs(v));
        assignment.assign(maxvar + 1, false);
        for (int v : lits) if (v > 0 && v < (int)assignment.size()) assignment[v] = true;
        fin.close(); return true;
    }

    void decode() {
        std::vector<bool> assignment;
        bool isSat = false;
        if (!readSatOutput(assignment, isSat)) { return; }

        std::ofstream fout(basename + ".metromap");
        if (!fout) { std::cerr << "Cannot open output metromap file" << std::endl; return; }

        if (!isSat) {
            fout << "0\n";
            fout.close();
            std::cerr << "UNSAT -> wrote 0" << std::endl;
            return;
        }

        // Calculate L_k values EXACTLY as in the encoder
        std::vector<long long> L_k_values;
        for (int k = 0; k < K; ++k) {
            int manhattan_dist = std::abs(starts[k].first - ends[k].first) + std::abs(starts[k].second - ends[k].second);
            long long lk = manhattan_dist + 2 * J + 4;
            L_k_values.push_back(std::min((long long)N * M, lk));
        }

        VarManager vm(N, M, K, L_k_values);
        
        for (int k = 0; k < K; k++) {
            long long L_k = L_k_values[k];
            int t_end = -1;

            // Find the termination step by checking the ReachedEnd variables
            for (long long t = 0; t < L_k; ++t) {
                int rend_var = vm.idReachedEnd(k, (int)t);
                if (rend_var < (int)assignment.size() && assignment[rend_var]) {
                    t_end = (int)t;
                    break;
                }
            }

            if (t_end == -1) {
                // This shouldn't happen in a valid SAT solution
                fout << "0\n"; 
                std::cerr << "Error: Could not find termination step for line " << k << std::endl;
                continue;
            }

            // Reconstruct path up to the determined end step
            std::vector<std::pair<int,int>> path(t_end + 1);
            for (int t = 0; t <= t_end; t++) {
                bool found_cell = false;
                for (int x = 0; x < N; x++) for (int y = 0; y < M; y++) {
                    int p_var = vm.idP(k, x, y, t);
                    if (p_var < (int)assignment.size() && assignment[p_var]) {
                        path[t] = {x, y};
                        found_cell = true;
                        break;
                    }
                }
                 if (!found_cell) { // Should also not happen
                    t_end = -2; 
                    break;
                }
            }

            if (t_end == -2) {
                fout << "0\n";
                std::cerr << "Error: Inconsistent path found for line " << k << std::endl;
                continue;
            }

            // Generate direction string from the reconstructed path
            for (int t = 0; t < t_end; ++t) {
                auto a = path[t];
                auto b = path[t + 1];
                if (b.first > a.first) fout << 'R';
                else if (b.first < a.first) fout << 'L';
                else if (b.second > a.second) fout << 'D';
                else if (b.second < a.second) fout << 'U';
            }
            fout << "0\n";
        }
        fout.close();
        std::cerr << "Decoded and wrote " << basename << ".metromap" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <basename>" << std::endl; return 1; }
    Decoder dec(argv[1]);
    if (!dec.readCity()) return 2;
    dec.decode();
    return 0;
}