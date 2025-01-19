#include <iostream>
#include <array>
#include <optional>
#include <tuple>
#include <cassert>
#include <vector>
#include <numeric>
#include <ranges>
#include <thread>
#include <chrono>
#include <sstream>

enum class FieldState {
    UNUSABLE,
    EMPTY,
    OCCUPIED,
};

template<size_t size>
using Board = std::array<std::array<FieldState, size>, size>;

template<size_t size>
Board<size> make_board() {
    static_assert(size % 2 == 1, "Board size must be odd.");

    Board<size> board = {};
    for (auto &row: board) {
        for (auto &cell: row) {
            cell = FieldState::OCCUPIED;
        }
    }

    for (auto index: std::array<size_t, 2>{{0, size - 1}}) {
        board[index][0] = FieldState::UNUSABLE;
        board[index][1] = FieldState::UNUSABLE;
        board[index][size - 2] = FieldState::UNUSABLE;
        board[index][size - 1] = FieldState::UNUSABLE;
    }

    for (auto index: std::array<size_t, 2>{1, size - 2}) {
        board[index][0] = FieldState::UNUSABLE;
        board[index][size - 1] = FieldState::UNUSABLE;
    }

    board[size / 2][size / 2] = FieldState::EMPTY;
    return board;
}

template<size_t size>
std::ostream &operator<<(std::ostream &os, const Board<size> &board) {

    for (const auto &row: board) {
        for (const auto &cell: row) {
            switch (cell) {
                case FieldState::UNUSABLE:
                    os << ' ';
                    break;
                case FieldState::EMPTY:
                    os << '.';
                    break;
                case FieldState::OCCUPIED:
                    os << '@';
                    break;
                default:
                    throw std::runtime_error("unknown cell type");
            }
        }
        os << '\n';
    }

    return os;
}

using Coordinate = std::tuple<size_t, size_t>;

struct Move {
    Coordinate from;
    Coordinate to;
};


std::ostream &operator<<(std::ostream &os, const Move &move) {
    auto [from_row, from_column] = move.from;
    auto [to_row, to_column] = move.to;
    os << "(" << from_row << ", " << from_column << ") ~> (" << to_row << ", " << to_column << ")";
    return os;
}

template<size_t size>
std::optional<FieldState> get_from_board(const Board<size> &board, int row, int column) {
    if (row < 0 || column < 0 || row >= size || column >= size) {
        return {};
    }

    return board[row][column];
}

template<size_t size>
std::optional<Move> get_move(const Board<size> &board, Coordinate offset) {
    auto directions = std::array<std::tuple<int, int>, 4>{std::tuple{0, 1}, {1, 0}, {-1, 0}, {0, -1}};

    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            auto [row_offset, column_offset] = offset;

            int offsetted_row = (row + row_offset) % size;
            int offsetted_column = (column + column_offset) % size;

            auto cell = get_from_board(board, offsetted_row, offsetted_column);
            assert(cell);
            if (cell.value() == FieldState::OCCUPIED) {
                for (const auto &[row_direction, column_direction]: directions) {
                    auto next_to = get_from_board(board, offsetted_row + row_direction,
                                                  offsetted_column + column_direction);
                    if (!next_to || (*next_to != FieldState::OCCUPIED)) { continue; }
                    auto target_row = offsetted_row + 2 * row_direction;
                    auto target_column = offsetted_column + 2 * column_direction;
                    auto over_one = get_from_board(board, target_row, target_column);
                    if (!over_one || (*over_one != FieldState::EMPTY)) { continue; }
                    return Move{{offsetted_row, offsetted_column},
                                {target_row,    target_column}};
                }
            }
        }
    }
    return {};
}


template<size_t size>
void do_move(Board<size> &board, const Move &move) {
    auto [from_row, from_column] = move.from;
    auto [to_row, to_column] = move.to;

    auto &from = board[from_row][from_column];
    assert(from == FieldState::OCCUPIED);
    from = FieldState::EMPTY;

    auto &to = board[to_row][to_column];
    assert(from == FieldState::EMPTY);
    to = FieldState::OCCUPIED;
    auto &over = board[std::midpoint(from_row, to_row)][std::midpoint(from_column, to_column)];
    assert(over == FieldState::OCCUPIED);
    over = FieldState::EMPTY;
}

template<size_t size>
int get_score(const Board<size> &board) {
    std::vector<int> results;
    std::transform(board.begin(), board.end(), std::back_inserter(results), [](const auto &v) {
        return std::count_if(v.begin(), v.end(), [](auto cell) { return cell == FieldState::OCCUPIED; });
    });
    auto result = std::accumulate(results.begin(), results.end(), 0);
    return result;
}

int run_simulation(size_t seed, bool print_run) {
    srand(seed);
    std::vector<Move> move_history;

    int constexpr board_size = 9;
    auto board = make_board<board_size>();

    size_t rx = rand();
    size_t ry = rand();
    Coordinate randomized_offset{rx, ry};

    auto possible_move = get_move(board, randomized_offset);
    if (print_run) {
        system("clear");
        std::cout << board;
    }
    int score = board_size * board_size - 13;
    while (possible_move) {
        if (print_run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            system("clear");
        }

        const auto &move = *possible_move;
        do_move(board, move);
        move_history.push_back(move);
        if (print_run) {
            std::cout << board;
        }
        size_t rx = rand();
        size_t ry = rand();
        Coordinate randomized_offset{rx, ry};
        possible_move = get_move(board, randomized_offset);
        --score;
    }


    if (print_run) {
        std::cout << "Using seed " << seed << ".\nEnded with " << score << " matches remaining. Took "
                  << move_history.size() << " moves:\n";
        char *sep = "";
        for (const auto &move: move_history) {
            std::cout << sep << move;
            sep = " ; ";
        }
        std::cout << '\n';
    }

    return score;
}

int main(int argc, const char *argv[]) {

    bool find = false;
    bool simulate = false;
    size_t seed = 0;

    if (argc == 2) {
        find = std::string("find") == argv[1];
    }
    if (argc == 3) {
        simulate = std::string("simulate") == argv[1];
    }
    if (simulate) {
        std::stringstream ss(argv[2]);
        if (!(ss >> seed)) {
            std::cerr << "unable to parse seed\n";
            return 1;
        }
    }

    if (!simulate && !find) {
        std::cerr << "usage: " << argv[0] << R"( <command> [seed]

    available commands:
        find            run until a solution with score 1 is found
        simulate        simulate a game from given seed

    arguments:
        seed            provide seed for a given simulation, only
                        used when command is "simulate".
                        use seed 0 for random seed.
)" << '\n';
        return 1;
    }
    if (seed == 0) {
        srand(time(nullptr));
        seed = rand();
    } else {
        srand(seed);
    }
    if (simulate) {
        run_simulation(seed, true);
        return 0;
    }
    if (find) {
        int score = 0;
        int n_iterations = 0;
        int best_score = INT_MAX;
        size_t best_seed = 0;
        int granularity = 100000;
        srand(time(nullptr));

        do {
            seed = rand();
            score = run_simulation(seed, false);
            n_iterations++;
            if (score < best_score) {
                best_score = score;
                best_seed = seed;
            }
            if ((n_iterations % granularity) == 0) {
                std::cout << "best score in " << n_iterations << " runs is " << best_score << " for seed " << best_seed
                          << '\n';
                n_iterations = 0;
                best_score = INT_MAX;
                best_seed = 0;
                srand(time(nullptr));
            }
        } while (score != 1);
        std::cout << "* * * winning seed is: " << seed << '\n';
        return 0;
    }

    return 0;
}