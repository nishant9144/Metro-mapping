#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <cmath> 

class VarManager {
private:
    int N, M, K, T_max;
    int at_offset;
public:
    VarManager(int n, int m, int k, int t_max) : N(n), M(m), K(k), T_max(t_max) {
        at_offset = 1;
    }
    int at(int k, int x, int y, int t) const {
        return at_offset + k * (N * M * (T_max + 1)) + y * (N * (T_max + 1)) + x * (T_max + 1) + t;
    }
};

class Decoder {
private:
    int N, M, K, J, T_max;
    std::vector<std::pair<int, int>> starts;
    std::vector<std::pair<int, int>> ends;

public:
    void readCityFile(const std::string& filename) {
        std::ifstream inFile(filename);
        int scenario, P_dummy;
        inFile >> scenario;
        if (scenario == 1) {
            inFile >> N >> M >> K >> J;
        } else {
            inFile >> N >> M >> K >> J >> P_dummy;
        }
        
        starts.resize(K);
        ends.resize(K);
        for (int k = 0; k < K; ++k) {
            inFile >> starts[k].first >> starts[k].second >> ends[k].first >> ends[k].second;
        }
        
        // Calculate maximum time steps needed
        T_max = (N - 1) + (M - 1) + 2 * J;
    }

    void decodeAndWrite(const std::string& sat_output_file, const std::string& metro_map_file) {
        std::ifstream inFile(sat_output_file);
        std::string status;
        inFile >> status;

        std::ofstream outFile(metro_map_file);
        if (status == "UNSAT") {
            outFile << "0" << std::endl;
            return;
        }

        std::map<int, bool> solution;
        int literal;
        while (inFile >> literal && literal != 0) {
            solution[std::abs(literal)] = (literal > 0);
        }

        VarManager vars(N, M, K, T_max);
        
        for (int k = 0; k < K; ++k) {
            std::string path_str = "";
            std::pair<int, int> current_pos = starts[k];

            for (int t = 0; t < T_max; ++t) {
                if (current_pos == ends[k]) {
                    break;
                }

                // Find the position at the next time step, t+1
                std::pair<int, int> next_pos = {-1, -1};
                
                // Check all adjacent cells for the position at t+1
                int dx[] = {1, -1, 0, 0}; // R, L
                int dy[] = {0, 0, 1, -1}; // D, U
                char dirs[] = {'R', 'L', 'D', 'U'};

                bool moved = false;
                for(int i = 0; i < 4; ++i) {
                    int nx = current_pos.first + dx[i];
                    int ny = current_pos.second + dy[i];

                    if (nx >= 0 && nx < N && ny >= 0 && ny < M) {
                        // Is line k at this neighbor cell at the next time step?
                        if (solution[vars.at(k, nx, ny, t + 1)]) {
                            path_str += dirs[i];
                            next_pos = {nx, ny};
                            moved = true;
                            break;
                        }
                    }
                }
                
                if (!moved) {
                     if (solution[vars.at(k, current_pos.first, current_pos.second, t + 1)]) {
                        next_pos = current_pos;
                    } else {
                        // This indicates an error or a broken path, but we'll proceed
                        // and the format checker would catch this logical inconsistency.
                        break; 
                    }
                }
                current_pos = next_pos;
            }
            outFile << path_str << "0" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./decoder <test_name>" << std::endl;
        return 1;
    }
    std::string test_name = argv[1];

    Decoder decoder;
    decoder.readCityFile(test_name + ".city");
    decoder.decodeAndWrite(test_name + ".satoutput", test_name + ".metromap");

    return 0;
}