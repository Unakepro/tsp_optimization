# TSP Optimization

Portfolio project for comparing heuristic algorithms on Euclidean Traveling Salesman Problem instances from TSPLIB.

The goal is to show clean C++17 engineering around an algorithms project: parsing real benchmark data, preserving tour correctness, running reproducible experiments, and reporting costs against known best solutions.

## Why TSP?

The Traveling Salesman Problem is a classic NP-hard optimization problem: given a set of cities and pairwise distances, find the shortest cycle that visits every city exactly once and returns to the start. It is small enough to explain clearly, but rich enough to demonstrate local search, stochastic optimization, benchmarking, and correctness testing.

## Implemented Algorithms

- **Simulated Annealing**: starts from a random tour and accepts improving moves, with probabilistic acceptance for worse moves as temperature cools.
- **Genetic Algorithm**: maintains a population of tours, uses order crossover, permutation-preserving mutation, elitism, and tournament-style parent selection.
- **Ant Colony Optimization**: builds tours with pheromone-weighted probabilistic transitions and reinforces shorter tours.
- **Two-Opt**: local search pass that reverses tour segments when it removes crossing or expensive edges.

## Architecture

- `Cities/city.hpp`, `Cities/city.cpp`: city model, TSPLIB parsing, EUC_2D distance, tour validation, total cost, distance matrix, two-opt.
- `sa/`: simulated annealing implementation and parameter validation.
- `genetic/`: GA implementation, order crossover, mutation, and parameter validation.
- `aco/`: ACO implementation, pheromone updates, and parameter validation.
- `main.cpp`: repeatable benchmark runner for the small TSPLIB suite.
- `hyperparameter_search/`: grid and random hyperparameter search with seeds, repeats, runtime, standard deviation, and known-optimum gaps.
- `tests/`: lightweight CTest executable covering parser, distance, tour validity, crossover, mutation, two-opt, and algorithm invariants.
- `tsplib/tests/`: bundled TSPLIB benchmark instances.
- `tsplib/solutions`: known best values used for gap reporting.

## Build

```bash
cmake --preset debug
cmake --build --preset debug
```

Without presets:

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
```

Release build:

```bash
cmake --preset release
cmake --build --preset release
```

Sanitizer build:

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --test-dir build/sanitizers --output-on-failure
```

## Test

```bash
ctest --test-dir build/debug --output-on-failure
```

The current tests check:

- TSPLIB parser accepts supported files and rejects duplicate city IDs.
- EUC_2D distance uses TSPLIB rounding.
- total tour cost includes the return-to-start edge.
- distance matrix cost is indexed by city ID, not vector position.
- tours contain each city exactly once.
- GA crossover and mutation preserve valid permutations.
- two-opt does not make a tour worse on a simple crossing case.
- SA, GA, and ACO return valid finite tours.

## Run Benchmarks

Main benchmark runner:

```bash
./build/debug/tsp_optimization 42
```

Argument:

- `42`: base random seed. If omitted, the default seed is `42`.

Outputs are written to `results/` with headers that include algorithm parameters, base seed, evaluation budget, cost statistic, and average runtime.

Hyperparameter search:

```bash
./build/debug/tsp_hyperparameter_search 42 3 tuning 10000
./build/debug/tsp_hyperparameter_search 42 5 final 100000
```

Arguments:

- `base_seed`
- `repeats`
- dataset set: `tuning` or `final`
- evaluation budget

Outputs are written to `search_results/*.csv` with parameter values, first run seed, best cost, mean cost, standard deviation, mean runtime, known best solution, and percent gap.

## Example Terminal Session

```text
$ cmake --preset debug
$ cmake --build --preset debug
$ ctest --test-dir build/debug --output-on-failure
100% tests passed

$ ./build/debug/tsp_hyperparameter_search 42 3 tuning 10000
Using base seed: 42
Using repeats: 3
Using dataset set: tuning
Using evaluation budget: 10000
...
```

## Benchmark Targets

Known best values are loaded from `tsplib/solutions`. These are the main small/final benchmark targets currently used by the project:

| Instance | Cities | Known Best | Used In |
| --- | ---: | ---: | --- |
| `eil51` | 51 | 426 | tuning, small benchmark |
| `berlin52` | 52 | 7542 | final, small benchmark |
| `st70` | 70 | 675 | tuning, small benchmark |
| `eil76` | 76 | 538 | final, small benchmark |
| `kroA100` | 100 | 21282 | final, small benchmark |
| `kroB100` | 100 | 22141 | final, small benchmark |
| `ch130` | 130 | 6110 | final benchmark |
| `pr144` | 144 | 58537 | tuning |

Recommended result summary after running a final benchmark:

| Algorithm | Instance | Runs | Best Cost | Mean Cost | Stddev | Known Best | Gap % | Mean Time |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ACO | `berlin52` | regenerate | regenerate | regenerate | regenerate | 7542 | regenerate | regenerate |
| SA | `berlin52` | regenerate | regenerate | regenerate | regenerate | 7542 | regenerate | regenerate |
| GA | `berlin52` | regenerate | regenerate | regenerate | regenerate | 7542 | regenerate | regenerate |

Generated benchmark CSV files are intentionally ignored by git so results can be regenerated instead of committed as stale artifacts.

## Reproducibility

- All stochastic algorithms use a shared seedable `std::mt19937`.
- Benchmark runs derive deterministic per-algorithm, per-dataset, per-repeat seeds from the base seed.
- Hyperparameter search records parameter seed and first run seed.
- CSV outputs include algorithm parameters, evaluation budget, repeats, runtime, mean, standard deviation, known best, and gap percent.
- CMake presets provide debug, release, and sanitizer builds.
- GitHub Actions builds and runs tests on every push and pull request.

## Limitations

- TSPLIB parser currently supports `EDGE_WEIGHT_TYPE: EUC_2D` with `NODE_COORD_SECTION`.
- These are heuristic solvers, not exact solvers, so they do not guarantee optimal tours.
- Simulated annealing still recomputes full tour cost for each proposed move.
- The main benchmark runner has fixed dataset and parameter choices; the hyperparameter executable is more configurable.
- Convergence logging is minimal and not yet emitted as a machine-readable CSV.

## Future Work

- Add a richer CLI for selecting algorithms, datasets, budgets, repeats, and output paths.
- Add convergence traces per repeat for plotting cost versus evaluation count.
- Add delta-cost evaluation for two-opt and simulated annealing moves.
- Add a final benchmark script that aggregates CSV outputs into a Markdown table automatically.
- Add support for more TSPLIB edge weight types where appropriate.
- Compare against a simple nearest-neighbor baseline.

## Repository Description

Deterministic C++17 benchmark suite for heuristic TSP solvers on TSPLIB instances, with SA, GA, ACO, two-opt, tests, and reproducible CSV reporting.
