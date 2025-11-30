# SYSC4001_A3P2

## Requirements
Linux

## Part 2a
This program simulates multiple TA marking exams concurrently using Unix processes, shared memory, and file I/O. Part 2a doesn't use semaphores. Race conditions are allowed and expected for Part 2a.

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

## Usage
On startup, the rubric and the first exam file are loaded into shared memory. All TA processes use only shared memory (not the files directly). Race conditions are expected.

Each TA reads the rubric. For all 5 rubric entries, it does the following:

- Sleeps 0.5–1.0 s

- Prints before/after each shared-memory read

- Randomly decides whether to correct the rubric

- If correcting, increments the rubric’s character (A -> B, C -> D, etc.)

- Prints before/after each shared-memory write

- Saves the updated rubric back to rubric.txt


Then the TA marks 5 questions in each exam. TAs select an unmarked question from shared memory. The program prints before/after each read/write. Marking takes 1.0–2.0 s. In this part, multiple TAs may mark the same question.

When all questions are marked, one TA loads the next exam file into shared memory. THe program prints all shared-memory writes. If the student number is 9999, sets terminate = 1 and all TAs stop.

It is expected in Part 2a that there are many simultaneous rubric reads/writes, lots of overlapping question markings, and several TAs may attempt to load the next exam. This is all due to race conditions, which are allowed in Part 2a.
