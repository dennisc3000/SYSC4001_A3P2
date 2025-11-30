# SYSC4001_A3P2

## Requirements
Linux

## Part 2A
This program simulates multiple TA marking exams concurrently using Unix processes, shared memory, and file I/O. Part 2A doesn't use semaphores. Race conditions are allowed and expected for Part 2A.

Compile with gcc:
```
gcc -Wall -O2 -o part2a part2a.c
```

Run the program with the correct parameter:
```
./part2a <num_TAs>
```
Where <num_TAs> is the number of TA processes to spawn (must be ≥ 2).

Ex.
```
./part2a 4
```
This starts 4 concurrent TA processes.
