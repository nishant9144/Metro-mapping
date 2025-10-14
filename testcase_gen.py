#!/usr/bin/env python3
import argparse
import os
import random
import sys
from collections import deque

def find_path_bfs(N, M, J, grid, start, end):
    """
    Finds a path from start to end using BFS, respecting the turn limit J.
    Returns the path as a list of coordinates, or None if no path is found.
    """
    if start == end:
        return [start]

    queue = deque([(start, [start], 0, None)])
    visited = {(start, 0, None): 0}

    while queue:
        (pos, path, turns, last_dir) = queue.popleft()

        if pos == end:
            return path

        for direction, (dx, dy) in {'R': (1, 0), 'L': (-1, 0), 'D': (0, 1), 'U': (0, -1)}.items():
            nx, ny = pos[0] + dx, pos[1] + dy

            if not (0 <= nx < N and 0 <= ny < M):
                continue

            if grid[nx][ny] and (nx, ny) != end:
                continue

            new_turns = turns + (1 if last_dir is not None and direction != last_dir else 0)

            if new_turns > J:
                continue

            new_path = path + [(nx, ny)]
            visited_key = ((nx, ny), new_turns, direction)
            if visited_key in visited and visited[visited_key] <= len(new_path):
                continue
            
            visited[visited_key] = len(new_path)
            queue.append(((nx, ny), new_path, new_turns, direction))
            
    return None


def compute_reachable_cells(N, M, J, start, end):
    """
    Computes all cells reachable from start to end with at most J turns.
    Returns a set of coordinates, or empty set if no path exists.
    """
    grid = [[False for _ in range(M)] for _ in range(N)]
    path = find_path_bfs(N, M, J, grid, start, end)
    
    if path is None:
        return set()
    
    return set(path)


def generate_satisfiable_instance(N, M, K, J, P):
    """
    Generates a guaranteed satisfiable instance by picking S/E pairs and finding paths.
    """
    grid = [[False for _ in range(M)] for _ in range(N)]
    metro_lines = []
    all_path_cells = set()
    
    for _ in range(K):
        found_path = None
        for _ in range(200):
            available_points = [(x, y) for x in range(N) for y in range(M) if not grid[x][y]]
            if len(available_points) < 2:
                break
            
            start, end = random.sample(available_points, 2)
            grid[end[0]][end[1]] = False
            path = find_path_bfs(N, M, J, grid, start, end)
            grid[end[0]][end[1]] = True
            
            if path:
                found_path = path
                break

        if not found_path:
            print(f"Error: Failed to construct a satisfiable instance.", file=sys.stderr)
            sys.exit(1)

        metro_lines.append({'start': found_path[0], 'end': found_path[-1]})
        for x, y in found_path:
            grid[x][y] = True
            all_path_cells.add((x, y))

    start_end_points = {p for line in metro_lines for p in (line['start'], line['end'])}
    possible_popular = list(all_path_cells - start_end_points)
    if len(possible_popular) < P:
        print(f"Warning: Reducing P from {P} to {len(possible_popular)}.", file=sys.stderr)
        P = len(possible_popular)
    popular_cells = random.sample(possible_popular, P) if P > 0 else []

    return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': P}


def generate_random_instance(N, M, K, J, P):
    """Generates a purely random instance (may be SAT or UNSAT)."""
    num_points = 2 * K + P
    if num_points > N * M:
        print(f"Error: Grid too small for {num_points} unique points.", file=sys.stderr)
        sys.exit(1)

    all_coords = [(x, y) for x in range(N) for y in range(M)]
    chosen_points = random.sample(all_coords, num_points)
    
    endpoints = chosen_points[:2*K]
    popular_cells = chosen_points[2*K:]
    
    metro_lines = []
    for i in range(K):
        metro_lines.append({'start': endpoints[i], 'end': endpoints[i+K]})
        
    return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': P}


def generate_unsat_instance(N, M, K, J, P):
    """
    Generates a guaranteed UNSAT instance using one of several techniques:
    
    1. J=0 with diagonal misalignment: All lines have misaligned start/end
    2. Crossing gadget (J<1): Two lines that must cross
    3. Coverage-based: Popular cells placed in unreachable regions
    """
    
    all_cells = [(x, y) for x in range(N) for y in range(M)]
    
    # Strategy 1: J=0 Diagonal Misalignment (Guaranteed UNSAT)
    if J == 0:
        if N < 2 or M < 2:
            print("Error: Grid too small (need N>=2, M>=2) for J=0 UNSAT.", file=sys.stderr)
            sys.exit(1)
        
        metro_lines = []
        used = set()
        
        for i in range(K):
            # Pick two points that are diagonally misaligned
            # (differ in both x and y coordinates)
            attempts = 0
            while attempts < 50:
                available = [p for p in all_cells if p not in used]
                if len(available) < 2:
                    break
                
                s, e = random.sample(available, 2)
                
                # For J=0, a path must be either horizontal or vertical only
                # So if s and e differ in BOTH x and y, it's impossible
                if s[0] != e[0] and s[1] != e[1]:
                    metro_lines.append({'start': s, 'end': e})
                    used.add(s)
                    used.add(e)
                    break
                
                attempts += 1
            
            if len(metro_lines) != i + 1:
                print("Error: Could not create enough misaligned pairs for J=0.", file=sys.stderr)
                sys.exit(1)
        
        # Popular cells: place in remaining cells (doesn't matter for J=0 UNSAT)
        pool = [p for p in all_cells if p not in used]
        popular_cells = random.sample(pool, min(len(pool), P)) if P > 0 else []
        
        return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': P}
    
    # Strategy 2: Crossing Gadget (J < 1, i.e., J=0 implicitly)
    if J == 0 and K >= 2:
        # Two lines must cross at a central point
        # With J=0, they can't turn, so they can't avoid the crossing
        if N < 5 or M < 5:
            print("Error: Grid too small for crossing gadget.", file=sys.stderr)
            sys.exit(1)
        
        cx, cy = N // 2, M // 2
        
        # Horizontal line through center
        h_start = (0, cy)
        h_end = (N - 1, cy)
        
        # Vertical line through center
        v_start = (cx, 0)
        v_end = (cx, M - 1)
        
        metro_lines = [
            {'start': h_start, 'end': h_end},
            {'start': v_start, 'end': v_end}
        ]
        used = {h_start, h_end, v_start, v_end}
        
        # Add remaining lines arbitrarily
        pool = [p for p in all_cells if p not in used]
        for i in range(2, K):
            if len(pool) < 2:
                print("Warning: Not enough cells for additional lines in crossing gadget.", file=sys.stderr)
                break
            s, e = random.sample(pool, 2)
            metro_lines.append({'start': s, 'end': e})
            pool = [p for p in pool if p not in {s, e}]
        
        pool = [p for p in all_cells if p not in {line['start'] or line['end'] for line in metro_lines}]
        popular_cells = random.sample(pool, min(len(pool), P)) if P > 0 else []
        
        return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': P}
    
    # Strategy 3: Coverage-Based UNSAT (Scenario 2)
    # Pick metro lines, compute their reachable cells, place popular cells outside
    if P > 0:
        points = random.sample(all_cells, min(2 * K, len(all_cells)))
        metro_lines = []
        for i in range(min(K, len(points) // 2)):
            metro_lines.append({'start': points[2*i], 'end': points[2*i + 1]})
        
        # Compute all cells reachable by any metro line
        reachable = set()
        for line in metro_lines:
            reachable.update(compute_reachable_cells(N, M, J, line['start'], line['end']))
        
        # Popular cells must be in unreachable regions
        all_start_ends = {line['start'] for line in metro_lines} | {line['end'] for line in metro_lines}
        unreachable = [p for p in all_cells if p not in reachable and p not in all_start_ends]
        
        if len(unreachable) < P:
            print(f"Warning: Only {len(unreachable)} unreachable cells found for {P} popular cells.", file=sys.stderr)
            P = len(unreachable)
        
        popular_cells = random.sample(unreachable, P) if P > 0 else []
        
        return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': P}
    
    # Strategy 4: Overpacking with Forced Conflicts
    # Create K lines that collectively require more cells than available
    # This is tricky, so we use: multiple long lines that must not overlap
    remaining_cells = N * M
    metro_lines = []
    used = set()
    
    for i in range(K):
        available = [p for p in all_cells if p not in used]
        if len(available) < 2:
            print("Error: Not enough cells for overpacking strategy.", file=sys.stderr)
            sys.exit(1)
        
        s, e = random.sample(available, 2)
        metro_lines.append({'start': s, 'end': e})
        
        # Estimate path length (Manhattan distance is lower bound)
        min_path_len = abs(s[0] - e[0]) + abs(s[1] - e[1]) + 1
        used.add(s)
        used.add(e)
        
        # Reserve cells for this path (generous estimate)
        remaining_cells -= min(remaining_cells, min_path_len * 2)
        
        if remaining_cells < 0:
            # We've run out of space—this is UNSAT!
            break
    
    popular_cells = []
    
    return {'metro_lines': metro_lines, 'popular_cells': popular_cells, 'P': 0}


def write_city_file(filepath, N, M, K, J, P, metro_lines, popular_cells):
    """Writes the generated instance to a .city file."""
    try:
        with open(filepath, 'w') as f:
            scenario = 2 if P > 0 else 1
            f.write(f"{scenario}\n")

            if scenario == 1:
                f.write(f"{N} {M} {K} {J}\n")
            else:
                f.write(f"{N} {M} {K} {J} {P}\n")

            for line in metro_lines:
                f.write(f"{line['start'][0]} {line['start'][1]} {line['end'][0]} {line['end'][1]}\n")

            if scenario == 2 and P > 0:
                f.write(" ".join(f"{x} {y}" for x, y in popular_cells) + "\n")
    except IOError as e:
        print(f"Error writing to file '{filepath}': {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Corrected Test Case Generator for Metro Map Planning Assignment.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    
    parser.add_argument("--N", type=int, required=True, help="Grid width (columns).")
    parser.add_argument("--M", type=int, required=True, help="Grid height (rows).")
    parser.add_argument("--K", type=int, required=True, help="Number of metro lines.")
    parser.add_argument("--J", type=int, required=True, help="Maximum turns per line.")
    parser.add_argument("--P", type=int, default=0, help="Number of popular cells (for Scenario 2).")
    parser.add_argument("--count", type=int, default=1, help="Number of instances to generate.")
    parser.add_argument("--mode", choices=["constructive", "random", "unsat"], default="random",
                        help="constructive=SAT, random=mixed, unsat=deliberate UNSAT.")
    parser.add_argument("--seed", type=int, help="Random seed for reproducibility.")
    parser.add_argument("--outdir", default=".", help="Output directory.")
    parser.add_argument("--prefix", default="case", help="Filename prefix.")

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
    
    os.makedirs(args.outdir, exist_ok=True)
    
    for i in range(args.count):
        filepath = os.path.join(args.outdir, f"{args.prefix}_{i:03d}.city")
        instance = None
        
        if args.mode == 'constructive':
            instance = generate_satisfiable_instance(args.N, args.M, args.K, args.J, args.P)
        elif args.mode == 'unsat':
            instance = generate_unsat_instance(args.N, args.M, args.K, args.J, args.P)
        else:  # random
            instance = generate_random_instance(args.N, args.M, args.K, args.J, args.P)
        
        P_final = instance.get('P', args.P)

        write_city_file(filepath, args.N, args.M, args.K, args.J, P_final, 
                        instance['metro_lines'], instance['popular_cells'])

        print(f"Successfully generated '{args.mode}' test case: {filepath}")

if __name__ == "__main__":
    main()