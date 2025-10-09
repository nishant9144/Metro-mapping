#include <iostream>
#include <vector>
#include <fstream>
#include <string>

struct Point {
    int x, y;
};

class Decoder {
private:
    int N, M, K;
    std::vector<std::pair<Point, Point>> lines;
    
    std::vector<std::vector<int>> grid; // grid[x][y] = k if line k is here, else -1

public:
    Decoder() : N(0), M(0), K(0) {}

    // Read the original problem to get dimensions and start/end points
    void readCityFile(const std::string& filename) {
        std::ifstream inFile(filename);
        int scenario, J_dummy, P_dummy;
        inFile >> scenario;
        inFile >> N >> M >> K >> J_dummy;
        if (scenario == 2) {
            inFile >> P_dummy;
        }

        lines.resize(K);
        for (int k = 0; k < K; ++k) {
            inFile >> lines[k].first.x >> lines[k].first.y >> lines[k].second.x >> lines[k].second.y;
        }
    }

    bool decodeSolution(const std::string& satoutput_filename) {
        std::ifstream inFile(satoutput_filename);
        std::string status;
        inFile >> status;

        if (status == "UNSAT") {
            return false;
        }

        grid.assign(N, std::vector<int>(M, -1));
        int var;
        int current_var_index = 1;
        
        // Reconstruct which cells are occupied by which lines
        for (int k = 0; k < K; ++k) {
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < M; ++j) {
                    inFile >> var;
                    if (var > 0) { // If P_k,i,j is true
                        grid[i][j] = k;
                    }
                }
            }
        }
        return true;
    }

    void writeOutput(const std::string& metromap_filename) {
        std::ofstream outFile(metromap_filename);

        for (int k = 0; k < K; ++k) {
            std::string path_str = "";
            Point current_pos = lines[k].first;
            Point end_pos = lines[k].second;
            
            // Simple path tracing
            std::vector<std::vector<bool>> visited(N, std::vector<bool>(M, false));
            
            while (current_pos.x != end_pos.x || current_pos.y != end_pos.y) {
                visited[current_pos.x][current_pos.y] = true;
                int cx = current_pos.x;
                int cy = current_pos.y;

                // Check Right
                if (cx + 1 < N && !visited[cx + 1][cy] && grid[cx + 1][cy] == k) {
                    path_str += 'R';
                    current_pos.x++;
                } 
                // Check Left
                else if (cx - 1 >= 0 && !visited[cx - 1][cy] && grid[cx - 1][cy] == k) {
                    path_str += 'L';
                    current_pos.x--;
                }
                // Check Down
                else if (cy + 1 < M && !visited[cx][cy + 1] && grid[cx][cy + 1] == k) {
                    path_str += 'D';
                    current_pos.y++;
                }
                // Check Up
                else if (cy - 1 >= 0 && !visited[cx][cy - 1] && grid[cx][cy - 1] == k) {
                    path_str += 'U';
                    current_pos.y--;
                } else {
                    // Should not happen in a valid solution
                    break;
                }
            }
            outFile << path_str << "0" << std::endl; 
        }
    }

    void writeUnsatOutput(const std::string& metromap_filename) {
        std::ofstream outFile(metromap_filename);
        outFile << "0" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./decoder <test_name>" << std::endl;
        return 1;
    }
    std::string test_name = argv[1];
    std::string city_file = test_name + ".city";
    std::string satoutput_file = test_name + ".satoutput";
    std::string metromap_file = test_name + ".metromap";

    Decoder decoder;
    decoder.readCityFile(city_file);

    if (decoder.decodeSolution(satoutput_file)) {
        decoder.writeOutput(metromap_file);
    } else {
        decoder.writeUnsatOutput(metromap_file);
    }

    return 0;
}