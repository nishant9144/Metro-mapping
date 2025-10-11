import sys
import os
import matplotlib
import matplotlib.patches as patches
import matplotlib.patheffects as patheffects
from matplotlib.patches import Circle, Rectangle, FancyArrowPatch
import numpy as np

from format_checker import (
    parse_city as fc_parse_city,
    parse_metromap as fc_parse_metromap,
    analyze_constraints as fc_analyze_constraints,
    short_summary as fc_short_summary,
    verbose_print as fc_verbose_print,
)
FORMAT_CHECKER_AVAILABLE = True


COLORS = [
    '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
    '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B739', '#52B788',
    '#E74C3C', '#3498DB', '#2ECC71', '#F39C12', '#9B59B6'
]

def parse_input(filename):
    """Parse the .city input file"""
    with open(filename, 'r') as f:
        lines = [l.strip() for l in f.readlines() if l.strip()]
    
    scenario = int(lines[0])
    params = list(map(int, lines[1].split()))
    N, M, K, J = params[0], params[1], params[2], params[3]
    P = params[4] if len(params) > 4 else 0
    
    metro_lines = []
    for i in range(K):
        parts = list(map(int, lines[2 + i].split()))
        metro_lines.append({
            'start': (parts[0], parts[1]),
            'end': (parts[2], parts[3])
        })
    
    popular_cells = []
    if scenario == 2 and P > 0:
        popular_parts = list(map(int, lines[2 + K].split()))
        for i in range(0, len(popular_parts), 2):
            popular_cells.append((popular_parts[i], popular_parts[i + 1]))
    
    return {
        'scenario': scenario,
        'N': N, 'M': M, 'K': K, 'J': J, 'P': P,
        'metro_lines': metro_lines,
        'popular_cells': popular_cells
    }

def parse_output(filename):
    """Parse the .metromap output file"""
    with open(filename, 'r') as f:
        lines = [l.strip() for l in f.readlines() if l.strip()]
    
    if len(lines) == 1 and lines[0] == '0':
        return {'unsatisfiable': True, 'paths': []}
    
    paths = []
    # The output from the C++ code might not have spaces, handle RRRDD0 format
    for line in lines:
        path_str = line.replace('0', '').strip()
        paths.append(list(path_str))
    
    return {'unsatisfiable': False, 'paths': paths}

def trace_path(start, moves):
    """Trace the path and return a list of (x, y) coordinates"""
    path_coords = []
    x, y = start
    path_coords.append((x, y))
    
    for move in moves:
        if move == 'R':
            x += 1
        elif move == 'L':
            x -= 1
        elif move == 'D':
            y += 1
        elif move == 'U':
            y -= 1
        path_coords.append((x, y))
    
    return path_coords

def visualize_metro_map(input_data, output_data, output_filename, show=True):
    """Generate visualization using matplotlib"""
    # Import pyplot after backend is selected
    import matplotlib.pyplot as plt
    N, M, K = input_data['N'], input_data['M'], input_data['K']
    scenario = input_data['scenario']
    popular_cells = input_data['popular_cells']
    
    # Create figure
    fig_width = max(12, N * 1.0)
    fig_height = max(8, M * 1.0 + 2)
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    
    # Set background color
    fig.patch.set_facecolor('white')
    ax.set_facecolor('white')
    
    if output_data['unsatisfiable']:
        # Display unsatisfiable message
        ax.text(0.5, 0.5, 'UNSATISFIABLE', 
                ha='center', va='center', 
                fontsize=40, color='#ef4444', weight='bold',
                transform=ax.transAxes)
        ax.text(0.5, 0.4, 'No valid solution exists for this configuration',
                ha='center', va='center',
                fontsize=16, color='#666',
                transform=ax.transAxes)
        ax.axis('off')
    else:
        # Draw grid cells
        for y in range(M):
            for x in range(N):
                # Determine cell color based on whether it's a popular cell
                if scenario == 2 and (x, y) in popular_cells:
                    # Popular cells get a distinct background color
                    facecolor = '#FFE5B4'  # Peach/light orange color
                    edgecolor = '#FF8C00'  # Dark orange border
                    linewidth = 3
                else:
                    # Regular cells
                    facecolor = 'white'
                    edgecolor = '#cccccc'
                    linewidth = 2
                
                # Draw cell background
                rect = Rectangle((x, y), 1, 1,
                                linewidth=linewidth,
                                edgecolor=edgecolor,
                                facecolor=facecolor)
                ax.add_patch(rect)
                
                # Add coordinate label
                ax.text(x + 0.08, y + 0.15, f'{x},{y}',
                       fontsize=8, color='#444444', alpha=1.0)

        # Draw paths as arrows
        for i, moves in enumerate(output_data['paths']):
            color = COLORS[i % len(COLORS)]
            path_coords = trace_path(input_data['metro_lines'][i]['start'], moves)
            
            # Convert grid coordinates to plot coordinates (center of the cell)
            plot_coords = [(x + 0.5, y + 0.5) for x, y in path_coords]
            
            # Draw arrow for each segment
            for j in range(len(plot_coords) - 1):
                x1, y1 = plot_coords[j]
                x2, y2 = plot_coords[j + 1]
                
                # Calculate direction vector
                dx = x2 - x1
                dy = y2 - y1
                length = np.sqrt(dx**2 + dy**2)
                
                # Normalize direction
                if length > 0:
                    dx_norm = dx / length
                    dy_norm = dy / length
                    
                    # Offset from start circle (first segment)
                    if j == 0:
                        x1 = x1 + dx_norm * 0.18
                        y1 = y1 + dy_norm * 0.18
                    
                    # Offset from end circle (last segment)
                    if j == len(plot_coords) - 2:
                        x2 = x2 - dx_norm * 0.18
                        y2 = y2 - dy_norm * 0.18
                
                arrow = FancyArrowPatch(
                    (x1, y1), (x2, y2),
                    arrowstyle='-|>',
                    mutation_scale=12,
                    linewidth=2,
                    color=color,
                    zorder=5
                )
                ax.add_patch(arrow)

        # Draw start and end points on top
        for i, line in enumerate(input_data['metro_lines']):
            sx, sy = line['start']
            ex, ey = line['end']
            color = COLORS[i % len(COLORS)]
            
            # Start point - filled circle with white border
            start_circle = Circle((sx + 0.5, sy + 0.5), 0.15,
                                 facecolor=color,
                                 edgecolor='white',
                                 linewidth=2,
                                 zorder=10)
            ax.add_patch(start_circle)
            
            # Add 'S' label
            ax.text(sx + 0.5, sy + 0.5, 'S',
                   ha='center', va='center',
                   fontsize=7, color='white', weight='bold',
                   zorder=11)
            
            # End point - filled circle with white border
            end_circle = Circle((ex + 0.5, ey + 0.5), 0.15,
                               facecolor=color,
                               edgecolor='white',
                               linewidth=2,
                               zorder=10)
            ax.add_patch(end_circle)
            
            # Add 'E' label
            ax.text(ex + 0.5, ey + 0.5, 'E',
                   ha='center', va='center',
                   fontsize=7, color='white', weight='bold',
                   zorder=11)
        
        # Set axis properties
        ax.set_xlim(-0.5, N + 0.5)
        ax.set_ylim(-0.5, M + 0.5)
        ax.set_aspect('equal')
        ax.invert_yaxis()
        
        # Set ticks to show column and row numbers
        ax.set_xticks(np.arange(0.5, N, 1))
        ax.set_yticks(np.arange(0.5, M, 1))
        ax.set_xticklabels([str(i) for i in range(N)])
        ax.set_yticklabels([str(i) for i in range(M)])
        ax.tick_params(length=0)
        
        # Add axis labels
        ax.set_xlabel('Column (X-axis)', fontsize=12, weight='bold')
        ax.set_ylabel('Row (Y-axis)', fontsize=12, weight='bold')
        
        # Title
        title_text = f'Metro Map Solution (Scenario {scenario})\nGrid: {N}×{M} | Lines: {K} | Max Turns: {input_data["J"]}'
        if scenario == 2:
            title_text += f' | Popular Cells: {input_data["P"]}'
        
        ax.set_title(title_text, 
                    fontsize=14, weight='bold', pad=20)
        
        # Add legend
        legend_elements = [
            patches.Patch(facecolor=COLORS[i % len(COLORS)], edgecolor='black', 
                          label=f'Line {i}') for i in range(K)
        ]
        
        # Add popular cell legend item for Scenario 2
        if scenario == 2 and len(popular_cells) > 0:
            legend_elements.append(
                patches.Patch(facecolor='#FFE5B4', edgecolor='#FF8C00', 
                             linewidth=2, label='Popular Cell')
            )
        
        legend = ax.legend(handles=legend_elements, 
                          title="Metro Lines" + (" & Cells" if scenario == 2 else ""),
                          loc='upper left', 
                          bbox_to_anchor=(1.02, 1),
                          fontsize=10,
                          framealpha=1.0)
        legend.get_frame().set_linewidth(1.5)
        legend.get_frame().set_edgecolor('black')
        
    plt.tight_layout(rect=(0, 0, 0.85, 1))
    plt.savefig(output_filename, dpi=150, bbox_inches='tight')
    print(f"Visualization saved to: {output_filename}")

    if show:
        plt.show()

def main():
    if len(sys.argv) < 3:
        print("Usage: python visualizer.py <input.city> <output.metromap>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    raw_flags = sys.argv[3:]
    flags = set(raw_flags)
    check_verbose = ('--check-verbose' in flags) or ('-v' in flags) or ('--verbose' in flags)

    backend_override = None
    for f in raw_flags:
        if f.startswith('--backend='):
            backend_override = f.split('=', 1)[1]
            break
    try:
        if backend_override:
            matplotlib.use(backend_override, force=True)
        else:
            # Prefer TkAgg if available
            matplotlib.use('TkAgg', force=True)
    except Exception as e:
        # Fall back to default backend
        print(f"(info) Could not set requested backend; using default. Reason: {e}")
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    if not os.path.exists(output_file):
        print(f"Error: Output file '{output_file}' not found")
        sys.exit(1)
    
    print("Parsing input file...")
    input_data = parse_input(input_file)
    
    print("Parsing output file...")
    output_data = parse_output(output_file)

    # Run format check using format_checker utilities
    if FORMAT_CHECKER_AVAILABLE:
        try:
            spec = fc_parse_city(input_file)
            state, metro_moves = fc_parse_metromap(output_file)
            if state == 'UNSAT':
                print('Output is UNSAT, as expected.')
            else:
                report = fc_analyze_constraints(spec, metro_moves)
                if check_verbose:
                    fc_verbose_print(report, spec)
                else:
                    for ln in fc_short_summary(report, spec):
                        print(ln)
                    print("FINAL VERDICT: %s" % ("VALID" if report['final_valid'] else "INVALID"))
        except Exception as e:
            print(f"(warning) Format check encountered an error: {e}")
    
    print("Generating visualization...")
    
    base_name = os.path.splitext(input_file)[0]
    output_image = f"{base_name}_viz.png"
    
    visualize_metro_map(input_data, output_data, output_image, show=True)

if __name__ == "__main__":
    main()

"""
    USAGE: python visualizer.py <input>.city <input>.metromap
"""