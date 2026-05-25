#include <cstdint>
#include <vector>
#include <Master.hpp>

#include <Logger.h> // uses simple-cpp-logger
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#endif

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define RESET   "\033[0m"

typedef uint8_t cell_t; // numbers 0 (empty cell) and 1-9
typedef uint16_t mask_t; // 9 bit numbers to show which integers are seen

class Sudoku {
private:
    static const int N = 9;
    static const int SIZE = 81;

    std::array<cell_t, SIZE> grid;
    uint8_t empty_count;
    std::array<bool, SIZE> empty_cells;
    
    // 9-bit masks for each block, row, column (e.g., 000100011 for 1,2,6 seen)
    std::array<mask_t, N> row_masks;
    std::array<mask_t, N> col_masks;
    std::array<mask_t, N> block_masks;

    static int block_index(int r, int c) {
        return (c / 3) + (r / 3) * 3;
    }

public:
    Sudoku(const Sudoku&) = default;
    Sudoku(std::array<cell_t, SIZE> grid_) {
        grid.fill(0); // will be set later to avoid invalid input
        empty_count = SIZE;
        empty_cells.fill(true);
        row_masks.fill(0);
        col_masks.fill(0);
        block_masks.fill(0);

        // try to set the grid
        for (int i = 0; i < SIZE; ++i) {
            if (grid_[i] == 0) continue;
            if (!set(i, grid_[i])) {
                throw std::runtime_error("Invalid grid provided for Sudoku");
            }
        }
    }
    
    bool set(int i, cell_t num) {
        if (num < 1 || num > N) return false;

        int r = i / 9;
        int c = i % 9;
        int b = block_index(r, c);
        
        if (!empty_cells[i]) return false; // non-empty cell

        mask_t bit = 1u << (num - 1);

        // check if this number is allowed
        if (
            (row_masks[r] & bit) ||
            (col_masks[c] & bit) ||
            (block_masks[b] & bit)
        ) return false;

        // add the number and adjust the masks
        grid[i] = num;
        empty_count--;
        empty_cells[i] = false;
        row_masks[r] |= bit;
        col_masks[c] |= bit;
        block_masks[b] |= bit;
        return true;
    }

    bool is_solved() {
        return empty_count == 0;
    }
    
    std::array<bool, SIZE> get_empty_cells() {
        return empty_cells;
    }

    std::array<cell_t, SIZE> get_grid() {
        return grid;
    }

    cell_t grid_at(int i) {
        return grid[i];
    }

    bool operator==(Sudoku other) {
        for (int i = 0; i < SIZE; ++i) {
            if (grid[i] != other.grid_at(i)) return false;
        }
        return true;
    }

    std::string to_string() {
        std::string res = "\n+-------+-------+-------+\n";
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                int idx = 9 * i + j;
                if (j % 3 == 0) res += "| ";
                res += std::to_string((int)grid[idx]) + " ";
            }
            res += "|\n";
            if (i == 2 || i == 5) res += "+-------+-------+-------+\n";
        }
        res += "+-------+-------+-------+";
        return res;
    }

    static std::vector<Sudoku> successors(Sudoku sudoku) {
        std::vector<Sudoku> res = {};

        if (sudoku.is_solved()) return res;

        // fill one more cell of the given sudoku
        for (int i = 0; i < SIZE; ++i) {
            if (sudoku.get_empty_cells()[i]) {
                // try to add all possible numbers
                for (cell_t num = 1; num <= N; ++num) {
                    Sudoku temp = Sudoku(sudoku);
                    if (temp.set(i, num)) res.push_back(temp);
                }
            }
        }
        return res;
    }

    static int map_for_count(Sudoku sudoku) {
        if (sudoku.is_solved()) return 1;
        return 0;
    }
    static int reduce_for_count(int x, int y) {
        return x + y;
    }
    static const int reduce_init_for_count = 0;
};



void count_solutions(size_t num_workers) {
    // std::array<cell_t, 81> grid = {
    //     3,  1,  6,      5,  7,  8,      4,  9,  2,
    //     5,  2,  9,      1,  3,  4,      7,  6,  8,
    //     4,  8,  7,      6,  2,  9,      5,  3,  1,

    //     2,  6,  3,      4,  1,  5,      9,  8,  7,
    //     9,  7,  4,      8,  6,  3,      1,  2,  5,
    //     8,  5,  1,      7,  9,  2,      6,  4,  3,

    //     1,  3,  8,      9,  4,  7,      2,  5,  6,
    //     6,  9,  2,      3,  5,  1,      8,  7,  4,
    //     7,  4,  5,      2,  8,  6,      3,  1,  9
    // };
    // TODO randomly clear a specific number of cells?

    std::array<cell_t, 81> grid = {
        3,  1,  6,      5,  7,  8,      4,  9,  2,
        5,  2,  9,      1,  3,  4,      7,  6,  8,
        4,  8,  7,      6,  2,  0,      0,  3,  1,

        2,  6,  3,      4,  1,  0,      0,  8,  7,
        9,  7,  4,      8,  6,  3,      1,  2,  5,
        8,  5,  1,      7,  9,  2,      6,  4,  3,

        1,  3,  8,      9,  4,  7,      2,  5,  6,
        6,  9,  2,      3,  5,  1,      8,  7,  4,
        7,  4,  5,      2,  8,  6,      3,  1,  9
    }; // expect 2 solutions
    Sudoku sudoku = Sudoku(grid);
    std::cout << sudoku.to_string() << std::endl;
    std::vector<Sudoku> seeds = {sudoku}; // single grid
    // std::vector<Sudoku> next = Sudoku::successors(sudoku);
    // std::cout << next.size() << std::endl;
    // for (int n = 0; n < next.size(); ++n) {
    //     std::cout << next[n].to_string() << std::endl;
    // }

    // count solutions
    auto cardinal_map = [](const Sudoku& x){ return 1; };
    auto cardinal_reduce = [](const int& x, const int& y){ return x + y; };
    int cardinal_reduce_init = 0;
    int cardinal_result = 1;

    Master<Sudoku, int> master1(
        num_workers,
        seeds,
        Sudoku::successors,
        Sudoku::map_for_count,
        Sudoku::reduce_for_count,
        Sudoku::reduce_init_for_count
    );
    int master1_res = master1.run2();
    if (cardinal_result == master1_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    }
}