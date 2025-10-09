#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

// Helper structure to represent a point
struct Point {
    int x, y;
};

// Represents a path segment (a straight line)
using PathSegment = std::vector<Point>;

// Represents a full path (a collection of segments)
using Path = std::vector<Point>;

class Encoder {
private:
    // Problem parameters
    int N, M, K, J, P;
    int scenario;
    std::vector<std::pair<Point, Point>> lines;
    std::vector<Point> popular_cells;

    // SAT related variables
    int var_count = 0;
    std::vector<std::vector<int>> clauses;
    
    // Variable mapping: P_k,x,y is true if line k is at (x,y)
    // We use a 3D vector for easy lookup.
    std::vector<std::vector<std::vector<int>>> p_vars; 

    // Variable mapping for path choices for each line
    std::vector<std::vector<int>> choice_vars;
    std::vector<std::vector<Path>> possible_paths;


public:
    Encoder() : N(0), M(0), K(0), J(0), P(0), scenario(0), var_count(0) {}

    void readInput(const std::string& filename) {
        std::ifstream inFile(filename);
        inFile >> scenario;
        inFile >> N >> M >> K >> J;
        if (scenario == 2) {
            inFile >> P;
        }

        lines.resize(K);
        for (int k = 0; k < K; ++k) {
            inFile >> lines[k].first.x >> lines[k].first.y >> lines[k].second.x >> lines[k].second.y;
        }

        if (scenario == 2) {
            popular_cells.resize(P);
            for (int p = 0; p < P; ++p) {
                inFile >> popular_cells[p].x >> popular_cells[p].y;
            }
        }
    }

    void generateClauses() {
        // --- 1. Initialize variables ---
        // Variable P_k,x,y is true if line k occupies cell (x, y)
        p_vars.resize(K, std::vector<std::vector<int>>(N, std::vector<int>(M)));
        for (int k = 0; k < K; ++k) {
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < M; ++j) {
                    p_vars[k][i][j] = ++var_count;
                }
            }
        }

        possible_paths.resize(K);
        choice_vars.resize(K);

        // --- 2. Generate all valid paths for each line and create choice variables ---
        for (int k = 0; k < K; ++k) {
            generatePathsForLine(k);
            for (size_t i = 0; i < possible_paths[k].size(); ++i) {
                choice_vars[k].push_back(++var_count);
            }
        }

        // --- 3. Add clauses ---
        for (int k = 0; k < K; ++k) {
            // Clause: Each line must choose exactly one path
            addExactlyOneChoiceClause(k);

            // Clause: If a path is chosen, all its cells must be active
            addPathImplicationClauses(k);
        }

        // Clause: A cell can be occupied by at most one line [cite: 11]
        addCellExclusivityClauses();

        // Clause (Scenario 2): Popular cells must be visited [cite: 53]
        if (scenario == 2) {
            addPopularCellClauses();
        }
    }

    void writeOutput(const std::string& filename) {
        std::ofstream outFile(filename);
        outFile << "p cnf " << var_count << " " << clauses.size() << std::endl;
        for (const auto& clause : clauses) {
            for (int literal : clause) {
                outFile << literal << " ";
            }
            outFile << "0" << std::endl;
        }
    }

private:
    // Generates all paths with at most J turns for a given line k
    void generatePathsForLine(int k) {
        Point start = lines[k].first;
        Point end = lines[k].second;

        // We use a recursive helper function to find paths
        findPathsRecursive(k, {start}, 0, (start.x == end.x)); 
    }

    void findPathsRecursive(int k, Path current_path, int turns, bool last_move_was_vertical) {
        Point current_pos = current_path.back();
        Point end_pos = lines[k].second;

        if (current_pos.x == end_pos.x && current_pos.y == end_pos.y) {
            possible_paths[k].push_back(current_path);
            return;
        }

        // Try moving horizontally
        if (current_pos.x != end_pos.x) {
            int new_turns = turns + (last_move_was_vertical ? 1 : 0);
            if (new_turns <= J) {
                Path new_path = current_path;
                int dx = (end_pos.x > current_pos.x) ? 1 : -1;
                for(int x = current_pos.x + dx; x != end_pos.x + dx; x += dx) {
                    new_path.push_back({x, current_pos.y});
                }
                new_path.push_back({end_pos.x, current_pos.y});
                findPathsRecursive(k, new_path, new_turns, false);
            }
        }

        // Try moving vertically
        if (current_pos.y != end_pos.y) {
            int new_turns = turns + (!last_move_was_vertical ? 1 : 0);
             if (new_turns <= J) {
                Path new_path = current_path;
                int dy = (end_pos.y > current_pos.y) ? 1 : -1;
                for (int y = current_pos.y + dy; y != end_pos.y + dy; y += dy) {
                    new_path.push_back({current_pos.x, y});
                }
                new_path.push_back({current_pos.x, end_pos.y});
                findPathsRecursive(k, new_path, new_turns, true);
             }
        }
    }


    // At least one choice must be true: (C1 v C2 v ... v Cn)
    // At most one choice is true: (-Ci v -Cj) for all i != j
    void addExactlyOneChoiceClause(int k) {
        // At least one path must be chosen
        if (!choice_vars[k].empty()) {
            clauses.push_back(choice_vars[k]);
        } else {
            // No possible paths for this line, so it's impossible. Add an empty clause for UNSAT.
             clauses.push_back({});
        }

        // At most one path can be chosen
        for (size_t i = 0; i < choice_vars[k].size(); ++i) {
            for (size_t j = i + 1; j < choice_vars[k].size(); ++j) {
                clauses.push_back({-choice_vars[k][i], -choice_vars[k][j]});
            }
        }
    }

    // Adds clauses: Choice_i => P_k,x,y for all (x,y) on path i
    // This is equivalent to: (-Choice_i v P_k,x,y)
    void addPathImplicationClauses(int k) {
        for (size_t i = 0; i < possible_paths[k].size(); ++i) {
            int choice_var = choice_vars[k][i];
            const auto& path = possible_paths[k][i];
            
            // A path is a sequence of points. We need to fill in the cells between them.
            for (size_t j = 0; j < path.size() - 1; ++j) {
                Point p1 = path[j];
                Point p2 = path[j+1];
                // Vertical segment
                if (p1.x == p2.x) {
                    for (int y = std::min(p1.y, p2.y); y <= std::max(p1.y, p2.y); ++y) {
                        clauses.push_back({-choice_var, p_vars[k][p1.x][y]});
                    }
                } 
                // Horizontal segment
                else if (p1.y == p2.y) {
                    for (int x = std::min(p1.x, p2.x); x <= std::max(p1.x, p2.x); ++x) {
                        clauses.push_back({-choice_var, p_vars[k][x][p1.y]});
                    }
                }
            }
        }
    }

    // Adds clauses: (-P_k1,x,y v -P_k2,x,y) for all k1 != k2
    void addCellExclusivityClauses() {
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < M; ++j) {
                for (int k1 = 0; k1 < K; ++k1) {
                    for (int k2 = k1 + 1; k2 < K; ++k2) {
                        clauses.push_back({-p_vars[k1][i][j], -p_vars[k2][i][j]});
                    }
                }
            }
        }
    }

    // Adds clause: (P_0,px,py v P_1,px,py v ... v P_K-1,px,py) for each popular cell
    void addPopularCellClauses() {
        for (const auto& cell : popular_cells) {
            std::vector<int> clause;
            for (int k = 0; k < K; ++k) {
                clause.push_back(p_vars[k][cell.x][cell.y]);
            }
            clauses.push_back(clause);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./encoder <test_name>" << std::endl;
        return 1;
    }
    std::string test_name = argv[1];
    std::string city_file = test_name + ".city";
    std::string satinput_file = test_name + ".satinput";

    Encoder encoder;
    encoder.readInput(city_file);
    encoder.generateClauses();
    encoder.writeOutput(satinput_file);

    return 0;
}