#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <numeric>

class VarManager {
private:
    int N, M, K, T_max;
    int var_count = 0;
    int at_offset;
    int occupies_offset;
    int move_h_offset;
    int move_v_offset;
    int turn_offset;
    int turn_sum_offset;
public:
    VarManager(int n, int m, int k, int t_max) : N(n), M(m), K(k), T_max(t_max) {
        at_offset = 1;
        occupies_offset = at_offset + K * N * M * (T_max + 1);
        move_h_offset = occupies_offset + K * N * M;
        move_v_offset = move_h_offset + K * (T_max);
        turn_offset = move_v_offset + K * (T_max);
        turn_sum_offset = turn_offset + K * (T_max);
        var_count = turn_sum_offset;
    }
    int at(int k, int x, int y, int t) const {
        return at_offset + k * (N * M * (T_max + 1)) + y * (N * (T_max + 1)) + x * (T_max + 1) + t;
    }
    int occupies(int k, int x, int y) const {
        return occupies_offset + k * (N * M) + y * N + x;
    }
    int move_h(int k, int t) const { return move_h_offset + k * T_max + t; }
    int move_v(int k, int t) const { return move_v_offset + k * T_max + t; }
    int turn(int k, int t) const { return turn_offset + k * T_max + t; }
    
    int turn_sum(int k, int t, int j) const {
        return turn_sum_offset + k * (T_max * 10) + t * 10 + j;
    }
    int total_vars() const { 
        return turn_sum_offset + K * T_max * 10;
    }
};

class Encoder {
private:
    int N, M, K, J, P, scenario, T_max;
    std::vector<std::pair<int, int>> starts, ends;
    std::vector<std::pair<int, int>> popular_cells;
    std::vector<std::vector<int>> clauses;
    
public:
    void readInput(const std::string& filename) {
        std::ifstream inFile(filename);
        inFile >> scenario;
        if (scenario == 1) {
            inFile >> N >> M >> K >> J;
        } else {
            inFile >> N >> M >> K >> J >> P;
        }
        starts.resize(K);
        ends.resize(K);
        for (int k = 0; k < K; ++k) {
            inFile >> starts[k].first >> starts[k].second >> ends[k].first >> ends[k].second;
        }
        if (scenario == 2) {
            popular_cells.resize(P);
            for (int p = 0; p < P; ++p) {
                inFile >> popular_cells[p].first >> popular_cells[p].second;
            }
        }
    }

    void generateClauses() {
        T_max = (N - 1) + (M - 1) + 2 * J;
        VarManager vars(N, M, K, T_max);

        // C1: Start and End constraints
        for (int k = 0; k < K; ++k) {
            clauses.push_back({vars.at(k, starts[k].first, starts[k].second, 0)});
            clauses.push_back({vars.at(k, ends[k].first, ends[k].second, T_max)});
        }
        // C2 & C3: Path validity and transitions
        for (int k = 0; k < K; ++k) {
            for (int t = 0; t <= T_max; ++t) {
                std::vector<int> at_least_one_pos;
                for (int x = 0; x < N; ++x) { for (int y = 0; y < M; ++y) at_least_one_pos.push_back(vars.at(k, x, y, t)); }
                clauses.push_back(at_least_one_pos);
                for (size_t i = 0; i < at_least_one_pos.size(); ++i) {
                    for (size_t j = i + 1; j < at_least_one_pos.size(); ++j) {
                        clauses.push_back({-at_least_one_pos[i], -at_least_one_pos[j]});
                    }
                }
                if (t < T_max) {
                    for (int x = 0; x < N; ++x) {
                        for (int y = 0; y < M; ++y) {
                            std::vector<int> possible_next_pos;
                            possible_next_pos.push_back(vars.at(k, x, y, t + 1));
                            if (x > 0) possible_next_pos.push_back(vars.at(k, x - 1, y, t + 1));
                            if (x < N - 1) possible_next_pos.push_back(vars.at(k, x + 1, y, t + 1));
                            if (y > 0) possible_next_pos.push_back(vars.at(k, x, y - 1, t + 1));
                            if (y < M - 1) possible_next_pos.push_back(vars.at(k, x, y + 1, t + 1));
                            std::vector<int> clause = possible_next_pos;
                            clause.push_back(-vars.at(k, x, y, t));
                            clauses.push_back(clause);
                        }
                    }
                }
            }
        }
        
        // C4: Define Occupies variables and add STRONG collision constraint
        for (int k = 0; k < K; ++k) {
            for (int x = 0; x < N; ++x) {
                for (int y = 0; y < M; ++y) {
                    std::vector<int> at_any_time_clause;
                    at_any_time_clause.push_back(-vars.occupies(k,x,y));
                    for (int t = 0; t <= T_max; ++t) {
                        clauses.push_back({-vars.at(k,x,y,t), vars.occupies(k,x,y)});
                        at_any_time_clause.push_back(vars.at(k,x,y,t));
                    }
                    clauses.push_back(at_any_time_clause);
                }
            }
        }
        for (int x = 0; x < N; ++x) {
            for (int y = 0; y < M; ++y) {
                for (int k1 = 0; k1 < K; ++k1) {
                    for (int k2 = k1 + 1; k2 < K; ++k2) {
                        clauses.push_back({-vars.occupies(k1, x, y), -vars.occupies(k2, x, y)});
                    }
                }
            }
        }

        // C5: Turn Constraints
        define_turn_clauses(vars);
        
        // C6: Popular Cells (Scenario 2)
        if (scenario == 2) {
            for (const auto& cell : popular_cells) {
                std::vector<int> popular_clause;
                for (int k = 0; k < K; ++k) {
                    popular_clause.push_back(vars.occupies(k, cell.first, cell.second));
                }
                clauses.push_back(popular_clause);
            }
        }
    }

    // The signature below is the important part - it now takes a const VarManager&
    void define_turn_clauses(const VarManager& vars) {
        for (int k = 0; k < K; ++k) {
            for (int t = 0; t < T_max; ++t) {
                if (t > 0) {
                    int m_h_prev = vars.move_h(k, t-1), m_v_prev = vars.move_v(k, t-1);
                    int m_h_curr = vars.move_h(k, t),   m_v_curr = vars.move_v(k, t);
                    int turn_curr = vars.turn(k, t);
                    clauses.push_back({-m_h_prev, -m_v_curr, turn_curr});
                    clauses.push_back({-m_v_prev, -m_h_curr, turn_curr});
                }
                for (int x=0; x<N; ++x){
                    for (int y=0; y<M; ++y){
                        if (x < N - 1) clauses.push_back({-vars.at(k,x,y,t), -vars.at(k,x+1,y,t+1), vars.move_h(k,t)});
                        if (x > 0)     clauses.push_back({-vars.at(k,x,y,t), -vars.at(k,x-1,y,t+1), vars.move_h(k,t)});
                        if (y < M-1)   clauses.push_back({-vars.at(k,x,y,t), -vars.at(k,x,y+1,t+1), vars.move_v(k,t)});
                        if (y > 0)     clauses.push_back({-vars.at(k,x,y,t), -vars.at(k,x,y-1,t+1), vars.move_v(k,t)});
                    }
                }
            }
            for (int t = 1; t < T_max; ++t) {
                for (int j = 1; j <= J + 1; ++j) {
                    int s_prev_j = (t > 1) ? vars.turn_sum(k, t - 1, j) : 0;
                    int s_prev_j_minus_1 = (t > 1 && j > 1) ? vars.turn_sum(k, t - 1, j - 1) : 0;
                    int turn_t = vars.turn(k, t);
                    int s_curr_j = vars.turn_sum(k, t, j);
                    if (t > 1) clauses.push_back({-s_prev_j, s_curr_j});
                    if (t > 1 && j > 1) clauses.push_back({-s_prev_j_minus_1, -turn_t, s_curr_j});
                    if (j==1) clauses.push_back({-turn_t, s_curr_j});
                }
            }
            clauses.push_back({-vars.turn_sum(k, T_max-1, J+1)});
        }
    }

    void writeOutput(const std::string& filename) {
        VarManager vars(N, M, K, T_max);
        std::ofstream outFile(filename);
        outFile << "p cnf " << vars.total_vars() << " " << clauses.size() << std::endl;
        for (const auto& clause : clauses) {
            for (int literal : clause) {
                outFile << literal << " ";
            }
            outFile << "0" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    std::string test_name = argv[1];
    Encoder encoder;
    encoder.readInput(test_name + ".city");
    encoder.generateClauses();
    encoder.writeOutput(test_name + ".satinput");
    return 0;
}