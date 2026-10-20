# AllOppsInC

Two C projects to explore simulations, artificial life, and emergent behaviour and wanted to add more fancy words but who cares .

## Projects

### 1. Cellular Automata — `automata.c`

A implementation of Conway's Game of Life using C and Raylib.

Cellular automata are grids of cells that follow simple rules. Each cell's next state depends on its current state and its neighbours. Repeating these rules can produce complex patterns from simple interactions.

Features include:
- Random cell generation
- Interactive cell placement and removal
- Glider spawning
- Pause and single-step controls
- Automatic seeding
- Live generation and population statistics
- Adjustable simulation speed

### 1. Battle Simulation — `life.c`

A multi-team battle simulation in which racial armies fight, adapt, retreat, and compete for survival.

Features include:
- Three competing teams
- Individual unit behaviour and combat
- Army formations and group awareness
- Retreat and outnumbered scenarios
- Dynamic combat conditions
- Sudden Death and a shrinking battlefield
- Live population statistics

##The simulation follows four rules:

1. A living cell with fewer than two living neighbours dies.
2. A living cell with two or three living neighbours survives.
3. A living cell with more than three living neighbours dies.
4. A dead cell with exactly three living neighbours becomes alive.

These simple rules create patterns that move, grow, stabilize, or disappear kinda just a simulated living cell.

## Requirements
- Laptop
- Hands
- Brain
- GCC
- Raylib
- Linux
- idk 

## Build and Run

Clone the repository:
git clone https://github.com/jagarlamudijanakirammaneesh/AllOppsInC.git
cd AllOppsInC
