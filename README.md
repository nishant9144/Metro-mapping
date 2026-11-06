# Metro Map Planning via SAT

> Routes K metro lines across an N×M city grid with no shared cells and at most J turns per line, by encoding the problem as CNF in **C++** and solving it with **MiniSat**.

---

## Table of Contents

- [Problem Statement](#problem-statement)
- [Pipeline](#pipeline)
- [SAT Encoding](#sat-encoding)
- [Decoding](#decoding)
- [Tech Stack](#tech-stack)
- [Build & Run](#build--run)
- [Input / Output Format](#input--output-format)
- [Testing & Tooling](#testing--tooling)
- [Project Structure](#project-structure)

---

## Problem Statement

The city is a grid with columns `x = 0..N-1`, rows `y = 0..M-1`, and `(0,0)` at the top-left. Each of the `K` proposed metro lines has a unique start `s_k` and end `e_k`. Find a path for every line such that:

1. At most **one metro line** runs under any cell.
2. Line `k` is a path from `s_k` to `e_k`.
3. Every line makes **at most `J` turns**.

**Scenario 2** adds `P` *popular cells*, and each one must be covered by at least one line.

The encoding has to be polynomial in the grid size, and the solver may be called **only once**. If no layout exists, the program outputs `0`.

---

## Pipeline

```
<base>.city ──► encoder ──► <base>.satinput ──► minisat ──► <base>.satoutput ──► decoder ──► <base>.metromap
  (run1.sh)                   (DIMACS CNF)                                       (run2.sh)        │
                                                                         format_checker.py / visualizer.py
```

MiniSat is invoked externally, between `run1.sh` and `run2.sh`. Neither script calls the solver.

---

## SAT Encoding

Implemented in `encoder.cpp`. The turn budget is built into the state variables, so no separate turn-counting circuit is needed.

### Variables

| Variable | Meaning | Count |
|----------|---------|-------|
| `use(k,x,y)` | Line `k` occupies cell `(x,y)` | `K·N·M` |
| `V(k,x,y,d,t)` | Line `k` **enters** `(x,y)` moving in direction `d ∈ {R,D,L,U}` after `t ∈ [0,J]` turns | `4·K·N·M·(J+1)` |
| auxiliary | Sinz sequential-counter variables | allocated on demand |

### Constraint Families

| # | Constraint | Clauses |
|---|------------|---------|
| 1 | Channelling: a state implies occupancy | `V(k,x,y,d,t) → use(k,x,y)` |
| 2 | An occupied non-start cell must have been entered | `use(k,x,y) → ⋁ V(k,x,y,·,·)` |
| 3 | **At most one line per cell** | Sinz sequential AMO over `use(·,x,y)` |
| 4 | At most one entry state per line per cell | Sinz sequential AMO over `V(k,x,y,·,·)` |
| 5 | Endpoints are occupied | unit clauses `use(k,s_k)`, `use(k,e_k)` |
| 6 | The start cell is never entered | `¬V(k,s_k,·,·)` |
| 7 | The line leaves the start with 0 turns | `⋁_d V(k, s_k+d, d, 0)` |
| 8 | The line never leaves the end | `¬V(k, e_k+d, d, ·)` |
| 9 | **Transition**: from any non-end state, take exactly one next step. Going straight keeps `t`; turning gives `t+1`; a step is allowed only if `t' ≤ J` | `V → ⋁ exits` plus pairwise `¬V ∨ ¬e_i ∨ ¬e_j` |
| 10 | *(Scenario 2)* Every popular cell is covered | `⋁_k use(k,p)` |

Because `t` never decreases and grows with each turn, a line can't use more than `J` turns; at `t = J`, no further turn is possible.

### Cardinality Encoding

The at-most-one constraints use **Sinz's sequential counter** (`n−1` auxiliary variables and O(n) clauses), not the O(n²) pairwise form. Clauses are streamed to a temporary `<base>.clauses.tmp`, then prefixed with the `p cnf <vars> <clauses>` header to produce `<base>.satinput`. The encoder prints the variable and clause counts to stderr.

---

## Decoding

`decoder.cpp` rebuilds the same variable numbering from the `.city` header and reads the MiniSat output:

- If the first line contains `UNSAT`, it writes a single `0`.
- Otherwise, it starts at `s_k` for each line, picks the true `V` neighbour with `t = 0`, then follows the true successor state (same direction → same `t`, turn → `t+1`) until it reaches `e_k`. It writes one move letter per step.

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Encoder / Decoder | C++ (g++, `-O3` via Makefile) |
| SAT Solver | MiniSat (external, not bundled) |
| Validation / Test gen | Python 3 (standard library) |
| Visualisation | Python 3 + Matplotlib + NumPy |
| Build | GNU Make + shell scripts |

---

## Build & Run

### Prerequisites

- `g++` that accepts C++17 structured bindings (`compile.sh` and the Makefile pass `-std=c++11`; `all_testcase_runner.py` compiles with `-std=c++17`)
- `minisat` on your `PATH`
- Python 3 (plus `matplotlib` and `numpy` for the visualiser)

### Step by step (the assignment's grading flow)

```bash
./compile.sh                                   # builds ./encoder and ./decoder
./run1.sh test                                 # test.city      -> test.satinput
minisat test.satinput test.satoutput           # solve
./run2.sh test                                 # test.satoutput -> test.metromap
python3 format_checker.py test                 # validate (add -v for per-line detail)
```

### With Make

```bash
make run FILE=test     # build, encode, minisat, decode, format-check
make viz FILE=test     # render test.city + test.metromap
make clean
```

`make run` exits with an error unless MiniSat returns status 10 (SAT) or 20 (UNSAT). Both `make` targets call `python`, not `python3`, so make sure `python` is on your `PATH`.

---

## Input / Output Format

### Input (`.city`)

```
2              # scenario (1 or 2)
8 6 2 1 2      # N M K J [P — scenario 2 only]
0 0 5 2        # line 0: sx sy ex ey
2 1 4 3        # line 1
1 0 3 1        # scenario 2: P popular cells as x y pairs on one line
```

The comments are only for explanation; real `.city` files contain just the numbers.

### Output (`.metromap`)

Each line `k` gets one row of moves (`L`, `R`, `U`, `D`), ending with `0`. `decoder.cpp` separates the tokens with spaces:

```
R R R R R D D 0
R R D D 0
```

An unsatisfiable instance produces a single `0`. `format_checker.py` accepts both the spaced form and the compact form (`RRRRRDD0`).

---

## Testing & Tooling

| Tool | Purpose |
|------|---------|
| `testcases_sat/` | 17 solvable instances (Scenarios 1 and 2, grids up to 100×100, up to 75 lines) |
| `testcases_unsat/` | 4 unsolvable instances |
| `all_testcase_runner.py` | Compiles the binaries if missing, then runs encode → `minisat` → decode → checker on every case in a temp dir. It expects `FINAL VERDICT: VALID` for SAT cases and `UNSAT` for UNSAT cases, and prints PASS/FAIL with timings. Press `T` to skip the current case. |
| `format_checker.py` | Checks C1 (no shared cells), C2 (each path runs from `s_k` to `e_k` and stays on the grid), C3 (turns ≤ J), and C4 (popular cells covered). Exits with 0 when the map is valid or UNSAT. |
| `visualizer.py` | Draws the grid, the coloured line paths and the popular cells, runs the format check, and saves `<base>_viz.png`. |
| `testcase_gen.py` | Generates `.city` files in three modes: `constructive` (guaranteed SAT, using a turn-limited BFS), `unsat` (built to be infeasible), and `random`. |

```bash
python3 all_testcase_runner.py

python3 testcase_gen.py --N 10 --M 10 --K 3 --J 1 --mode constructive
python3 testcase_gen.py --N 15 --M 15 --K 5 --J 0 --mode unsat --count 3 --outdir unsat_cases
# optional: --P <popular cells> --seed <n> --prefix <name>   (output: <prefix>_000.city, ...)

python visualizer.py test.city test.metromap [-v]
```

---

## Project Structure

```
.
├── encoder.cpp              # .city -> DIMACS CNF (state-based encoding, Sinz AMO)
├── decoder.cpp              # MiniSat model -> .metromap
├── correct_enc.cpp          # Alternative time-step encoding (P(k,x,y,t)); not used by any script
├── correct_dec.cpp          # Decoder for the alternative encoding
├── compile.sh               # Builds encoder + decoder
├── run1.sh / run2.sh        # Encode / decode wrappers
├── Makefile                 # build, run, viz, clean
├── format_checker.py        # Constraint validator
├── visualizer.py            # Matplotlib renderer
├── testcase_gen.py          # Instance generator
├── all_testcase_runner.py   # End-to-end test harness
├── testcases_sat/           # Satisfiable .city instances
├── testcases_unsat/         # Unsatisfiable .city instances
├── A3.pdf                   # Assignment specification
└── readme.pdf               # Test-generator documentation
```

> `correct_enc.cpp` is an alternative formulation, kept alongside the shipped encoder, that tracks each line's cell at every time step (`K·N·M·(N·M)` position variables, pairwise at-most-one). It stops with an error once that count exceeds 2×10⁸, which rules out the larger shipped instances. The state-based `encoder.cpp` drops the time dimension entirely.

---

## License

Developed for **Assignment 3: Metro Map Planning** (SAT formulation). Academic use only.
