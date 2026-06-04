# Overview
This program performs Map-Reduce on recursively enumerated sets.

# Usage
The program is compiled with `cmake`:
```bash
mkdir build
cd build
cmake ..
make
```
Three test cases can be run with our program.

## Basic correctness
Basic correctness tests can be executed with
```
./basic_test <num_workers>
```
With 1-20 workers. This runs the sequential and parallel implementations (with 3 types of work-stealing) on the following problem:
- The set S is linear combinations of 3 and 5 between 0 and 30. The seeds are 0 and 30.
- Four Map-Reduce configurations are run:
1. Cardinality of S
2. How many even numbers there are in S?
3. The maximum number in S
4. Are all numbers in S even?

## Sudoku solving
The Sudoku solving test runs the program on 5 different puzzles, 5 times each. The computation time and results are output for each run, and an average (for each type of execution) is given at the end. **Warning:** tests with many empty cells in the original sudoku grid may take some time to run, especially for the sequential algorithm. The program can be run as follows:
```
./sudoku_test <number of workers> [-once] [-blanks <number of empty cells>] [-seq] [-noseq] [-steal <steal type>]
```
By default, the test is executed 5 times for 5 puzzles sequentially and in parallel with each work-stealing type on a sudoku with 10 empty cells.

Argument sepcifications:
- at least 1 and at most 50 workers can be used
- "-once" runs a single test on a single puzzle (rather than 25 tests total)
- number of empty cells should be at most 20 (over 14 not recommended for sequential program)
- "-seq" will run the sequential algorithm only (dummy number of workers still needed)
- "-noseq" will run all but the sequential algorithm (recommended for large tests)
- steal type (incompatible with "-seq") should be 0 (no work-stealing), 1 (naive work-stealing), or 2 (smart work-stealing), then the test will be run only with that steal type

## Graph exploraiton