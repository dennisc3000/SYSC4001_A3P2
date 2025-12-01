# SYSC4001_A3P2
Lab section L3

Dennis Chen 
student#: 101236818

Mithushan Ravichandramohan 
student#: 101262467


## Requirements
Linux

## Usage
This program simulates multiple TA marking exams concurrently using Unix processes, shared memory, and file I/O. Part 2a doesn't use semaphores. Race conditions are allowed and expected for Part 2a.

Compile with gcc:
```
gcc -Wall -O2 -o part2a part2a.c
gcc -Wall -O2 -o part2b part2b.c
```
Part 2a and Part 2b **should not be run simultaneously, only run one at a time.**

Run the program with the number of TAs you want:
```
./part2a <num_TAs>
./part2b <num_TAs>
```
Where <num_TAs> is the number of TA processes to spawn (must be ≥ 2).
Ex.
```
./part2a 4
./part2b 4
```
This starts 4 concurrent TA processes.

**IMPORTANT**
**After running each test case, manually reset the rubric file to its original state:**
```
1, A
2, B
3, C
4, D
5, E
```
