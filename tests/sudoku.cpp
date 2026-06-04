#include <cstdint>
#include <utility>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <Master.hpp>
#include <RecursivelyEnumeratedSet.hpp>

// #include <Logger.h> // uses simple-cpp-logger
// #ifdef NO_LOGS
// #define LogInfo(...) ((void)0)
// #endif

#define MIN_WORKERS 1
#define MAX_WORKERS 50

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define RESET   "\033[0m"

typedef uint8_t cell_t; // numbers 0 (empty cell) and 1-9
typedef uint16_t mask_t; // 9 bit numbers to show which integers are seen

class Sudoku {
public:
    static constexpr int N_WORDS = 6; // 64-bit words (6*64=384, while 4*81=324 needed)
    std::array<uint64_t, N_WORDS> packed_grid{};
private:
    static constexpr int N = 9;
    static constexpr int SIZE = 81;

    // for hashing
    static constexpr int CELL_BITS = 4; // each number is 9 at most (so 0b1001)
    static constexpr int CELL_BITS_FULL = 0b1111;
    static constexpr int WORD_SIZE = 64;

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
    Sudoku() {
        // empty sudoku
        grid.fill(0);
        empty_count = SIZE;
        empty_cells.fill(true);
        row_masks.fill(0);
        col_masks.fill(0);
        block_masks.fill(0);
    }

    static std::array<cell_t, SIZE> string_to_grid(std::string repr) {
        if (repr.length() != 81) throw std::runtime_error("Invalid Sudoku representation");

        std::array<cell_t, SIZE> res;
        res.fill(0);
        for (int i = 0; i < repr.length(); ++i) {
            cell_t num = (cell_t)repr[i] - 48;
            res[i] = num;
        }
        return res;
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

        // set the packed value
        int bit_pos = i * CELL_BITS;
        int word_idx = bit_pos / WORD_SIZE;
        int offset_in_word = bit_pos % WORD_SIZE;
        // XXX packed_grid[word_idx] &= ~((uint64_t)CELL_BITS_FULL << offset_in_word); // clear the value
        packed_grid[word_idx] |= ((uint64_t)num << offset_in_word);

        return true;
    }

    bool is_solved() const {
        return empty_count == 0;
    }
    
    std::array<bool, SIZE> get_empty_cells() const {
        return empty_cells;
    }

    std::array<cell_t, SIZE> get_grid() const {
        return grid;
    }

    cell_t grid_at(int i) const {
        return grid[i];
    }

    std::array<uint64_t, N_WORDS> get_packed_grid() const {
        return packed_grid;
    }

    bool operator==(const Sudoku& other) const {
        return packed_grid == other.get_packed_grid();
    }

    std::string to_string() const {
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

    static std::vector<Sudoku> successors(const Sudoku& sudoku) {
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

// hashing function of sudoku (to allow unordered_set use)
template<>
struct std::hash<Sudoku> {
    std::size_t operator()(const Sudoku& sudoku) const {
        const uint64_t* data = sudoku.get_packed_grid().data();
        uint64_t res = data[0]; // first word

        for (int i = 0; i < Sudoku::N_WORDS; ++i) {
            // 64-bit magic constant 0x9e3779b97f4a7c15
            res ^= data[i] + 0x9e3779b97f4a7c15 + (res << 6) + (res >> 2);
        }
        return res;
    }
};

void incorrect_usage() {
    std::cout << "Usage: ./sudoku_test <number of workers> [-once] [-blanks <number of empty cells>] [-seq] [-noseq] [-steal <steal type>]" << std::endl;
    std::cout << "  Default: the \"count number of solutions\" test is executed 5 times for 5 puzzles sequentially and in parallel with each work-stealing type on a sudoku with 10 empty cells" << std::endl;
    std::cout << "  Argument sepcifications:" << std::endl;
    std::cout << "    - at least " << MIN_WORKERS << " and at most " << MAX_WORKERS << " workers can be used" << std::endl;
    std::cout << "    - \"-once\" runs a single test on a single puzzle (rather than 25 tests total)" << std::endl;
    std::cout << "    - number of empty cellc should be at most " << 20 << " (over 14 not recommended for sequential program)" << std::endl;
    std::cout << "    - \"-seq\" will run the sequential algorithm (dummy number of workers still needed)" << std::endl;
    std::cout << "    - \"-noseq\" will run all but the sequential algorithm" << std::endl;
    std::cout << "    - steal type (incompatible with \"-seq\") should be 0 (no work-stealing), 1 (naive work-stealing), or 2 (smart work-stealing)" << std::endl;
}

struct Config {
    int num_workers;
    // fill with default values
    bool once = false;
    int blanks = 10;
    bool test_seq_only = false;
    bool test_seq = true;
    int steal_type = -1; // -1 means test each type
};

Config parse_args(int argc, char** argv) {
    if (argc < 2) {
        incorrect_usage();
        throw std::runtime_error("No number of workers provided");
    }

    Config config;
    
    // number of workers
    try {
        config.num_workers = std::stoi(argv[1]);
        if (config.num_workers > MAX_WORKERS || config.num_workers < MIN_WORKERS) {
            incorrect_usage();
            throw std::runtime_error("Invalid number of workers");
        }
    } catch (...) {
        incorrect_usage();
        throw std::runtime_error("Invalid number of workers");
    }

    // optional arguments
    int i = 2;
    while (i < argc) {
        std::string arg = argv[i];

        // check each possible argument
        if (arg == "-once") {
            config.once = true;
            ++i;
        } else if (arg == "-seq") {
            config.test_seq_only = true;
            ++i;
        } else if (arg == "-blanks") {
            if (i + 1 == argc) {
                incorrect_usage();
                throw std::runtime_error("No number of empty cells provided after -blanks");
            }
            try {
                int blanks = std::stoi(argv[i + 1]);
                if (blanks < 0 || blanks > 20) {
                    incorrect_usage();
                    throw std::runtime_error("Invalid number of empty cells");
                }
                config.blanks = blanks;
            } catch (...) {
                incorrect_usage();
                throw std::runtime_error("Invalid number of empty cells");
            }
            i += 2;
        } else if (arg == "-steal") {
            if (i + 1 == argc) {
                incorrect_usage();
                throw std::runtime_error("No steal type provided after -steal");
            }
            std::string steal_type = argv[i + 1];
            if (steal_type != "0" && steal_type != "1" && steal_type != "2") {
                incorrect_usage();
                throw std::runtime_error("Invalid steal type");
            }
            config.steal_type = std::stoi(steal_type);
            i += 2;
        } else if (arg == "-noseq") {
            config.test_seq = false;
            ++i;
        } else {
            incorrect_usage();
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    // ensure not steal type and sequential testing simulteneously
    if (config.steal_type != -1 && config.test_seq_only) {
        incorrect_usage();
        throw std::runtime_error("-seq and -steal are incompatible");
    }

    return config;
}

void count_solutions(Config config) {
    auto map = [](const Sudoku& x){
        if (x.is_solved()) return 1;
        return 0;
    };
    auto reduce = [](const int& x, const int& y){ return x + y; };
    int reduce_init = 0;

    // full grids
    // source: https://www.kaggle.com/datasets/rohanrao/sudoku
    std::vector<std::string> full_grids = {
        "679518243543729618821634957794352186358461729216897534485276391962183475137945862",
        "371986524846521379592473861463819752285347916719652438634195287128734695957268143",
        "748391562365248791912675483421786935589413276673529814834962157296157348157834629",
        "298317645764285139153946278327168954981453726645792813539821467872634591416579382",
        "142895637975136824836742519398467152451328796267519348529673481613284975784951263"
    };
    int N_ITERS = 5;

    int n_blanks = config.blanks;
    std::cout << "Solving sudoku with " << n_blanks << " empty cells" << std::endl;

    // create the Sudoku objects
    std::vector<Sudoku> full_sudokus{};
    std::vector<Sudoku> test_sudokus{};
    for (std::string full_grid : full_grids) {
        // randomly choose empty cell positions
        std::vector<int> positions{};
        while (positions.size() < n_blanks) {
            int candidate = rand() % 81;
            if (std::find(positions.begin(), positions.end(), candidate) == positions.end()) positions.push_back(candidate);
        }
        // for (int p : positions) std::cout << p << " ";
        // std::cout << std::endl;

        std::string grid_str = full_grid;
        // remove certain numbers
        for (int p : positions) grid_str[p] = '0';

        full_sudokus.push_back(Sudoku(Sudoku::string_to_grid(full_grid)));

        Sudoku sudoku = Sudoku(Sudoku::string_to_grid(grid_str));
        // std::cout << sudoku.to_string() << std::endl;
        test_sudokus.push_back(sudoku);
    }

    // testing
    std::vector<std::string> steal_name = {"no", "naive", "smart"};
    std::vector<size_t> search_space{};
    std::vector<int64_t> time_seq{};
    std::vector<std::vector<int64_t>> time_par(3);
    int bound = config.once ? 1 : N_ITERS * full_grids.size();
    for (int j = 0; j < bound; ++j) {
        int i = j % full_grids.size();

        //std::vector<Sudoku> seeds = {sudoku};
        // start after one round of successors (for no stealing tests)
        std::vector<Sudoku> seeds = Sudoku::successors(test_sudokus[i]);
        size_t count = 0;
        size_t *tasks_processed = &count;
        auto parallel_test = [
            steal_name, config, seeds, map, reduce, reduce_init, tasks_processed
        ](int steal_type) {
            std::cout << "Parallel with " << steal_name[steal_type] << " work-stealing" << std::endl;

            Master<Sudoku, int> master(
                config.num_workers,
                seeds,
                Sudoku::successors,
                map,
                reduce,
                reduce_init,
                steal_type
            );
            auto start = std::chrono::steady_clock::now();
            int res = master.run();
            auto finish = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
            std::cout << elapsed << " μs (" << elapsed / 1000 << " ms)" << std::endl;
            std::cout << "Result: " << res << std::endl;

            // check the total number of successors processed
            if (*tasks_processed == 0) *tasks_processed = master.get_visited_set().size();
            std::cout << "==============================" << std::endl;

            std::pair<int, uint64_t> pair(res, elapsed);
            return pair;
        };

        // only one parallel test
        if (config.steal_type != -1) {
            uint64_t time = parallel_test(config.steal_type).second;
            // record the time and search space size
            time_par[config.steal_type].push_back(time);
            search_space.push_back(count);
            continue;
        }

        std::vector<int> results = {}; // record all results

        if (config.test_seq) {
            // sequential tests
            std::cout << "Sequential" << std::endl;
            RESetMapReduce<Sudoku, int> re(seeds, Sudoku::successors);
            auto start = std::chrono::steady_clock::now();
            int res =
                re.map_reduce_avoid_duplicate(map,reduce, reduce_init);
            auto finish = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
            std::cout << elapsed << " μs (" << elapsed / 1000 << " ms)" << std::endl;
            time_seq.push_back(elapsed);
            std::cout << "Result: " << res << std::endl;
            results.push_back(res);
            std::cout << "==============================" << std::endl;
            // stop if only sequential test requested
            if (config.test_seq_only) continue;
        }

        // test each work-stealing
        for (int i = 0; i < 3; ++i) {
            std::pair<int, uint64_t> res_and_time = parallel_test(i);
            results.push_back(res_and_time.first);
            time_par[i].push_back(res_and_time.second);
        }

        // record the search space size
        search_space.push_back(count);

        // ensure all results match
        int res1 = results[0];
        for (int i = 1; i < results.size(); ++i) {
            if (results[i] != res1) throw std::runtime_error("DIFFERENT RESULTS");
        }
    }

    // average search space size
    if (!config.test_seq_only) {
        size_t sum = 0;
        for (size_t s : search_space) sum += s;
        std::cout << "Average search space size: " << sum / search_space.size() << std::endl;
    }

    // average time
    if (config.steal_type != -1) {
        uint64_t sum = 0;
        for (uint64_t t : time_par[config.steal_type]) sum += t;
        std::cout << "Parallel with " << steal_name[config.steal_type] << " work-stealing average time (μs): " << sum / time_par[config.steal_type].size() << std::endl;
        return;
    }

    if (config.test_seq) {
        // sequential results
        uint64_t sum = 0;
        for (uint64_t t : time_seq) sum += t;
        std::cout << "Sequential average time (μs): " << sum / time_seq.size() << std::endl;
    }
    
    // all parallel results
    if (!config.test_seq_only) {
        for (int i = 0; i < 3; ++i) {
            uint64_t sum = 0;
            for (uint64_t t : time_par[i]) sum += t;
            std::cout << "Parallel with " << steal_name[i] << " work-stealing average time (μs): " << sum / time_par[i].size() << std::endl;
        }
    }
}

int main(int argc, char **argv) {
    Config config = parse_args(argc, argv);

    count_solutions(config);

    return 0;
}