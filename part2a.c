// Part 2a) (no semaphores)

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define NUM_Q 5

typedef enum {
    Q_NOT_MARKED = 0,
    Q_IN_PROGRESS = 1,
    Q_DONE = 2
} qstate_t;

typedef struct {
    char rubric_text[NUM_Q][32];  // store each rubric line as a small string
    int  current_exam_index;      // index into exam_files[]
    int  student_number;          // current exam's student number
    qstate_t question_state[NUM_Q];
    int  terminate;               // 0 = keep going, 1 = stop (9999 reached)
} shared_t;

/*CONFIG*/

// Each file should contain a 4-digit student number, e.g. "0001".
static const char *exam_files[] = {
    "exam_files/exam01.txt", "exam_files/exam02.txt", "exam_files/exam03.txt", "exam_files/exam04.txt",
    "exam_files/exam05.txt", "exam_files/exam06.txt", "exam_files/exam07.txt", "exam_files/exam08.txt",
    "exam_files/exam09.txt", "exam_files/exam10.txt", "exam_files/exam11.txt", "exam_files/exam12.txt",
    "exam_files/exam13.txt", "exam_files/exam14.txt", "exam_files/exam15.txt", "exam_files/exam16.txt",
    "exam_files/exam17.txt", "exam_files/exam18.txt", "exam_files/exam19.txt", "exam_files/exam20.txt"
};
static const int num_exams = sizeof(exam_files) / sizeof(exam_files[0]);
static const char *rubric_filename = "rubric.txt";

/*UTILS*/

static double urand01(void) {
    // uniform [0,1)
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

static void sleep_random(double min_s, double max_s) {
    double span = max_s - min_s;
    double s = min_s + urand01() * span;
    if (s < 0) s = 0;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/*RUBRIC I/O*/

static void load_rubric_into_shared(shared_t *sh) {
    FILE *f = fopen(rubric_filename, "r");
    if (!f) die("fopen rubric.txt");

    char buf[128];
    for (int i = 0; i < NUM_Q; ++i) {
        if (!fgets(buf, sizeof(buf), f)) {
            fprintf(stderr, "rubric.txt has fewer than %d lines\n", NUM_Q);
            fclose(f);
            exit(EXIT_FAILURE);
        }
        // store trimmed
        buf[strcspn(buf, "\r\n")] = '\0';
        snprintf(sh->rubric_text[i], sizeof(sh->rubric_text[i]), "%s", buf);
    }
    fclose(f);
}

static void save_rubric_from_shared(shared_t *sh) {
    FILE *f = fopen(rubric_filename, "w");
    if (!f) {
        perror("fopen rubric.txt for write");
        return;
    }
    for (int i = 0; i < NUM_Q; ++i) {
        fprintf(f, "%s\n", sh->rubric_text[i]);
    }
    fclose(f);
}

/*EXAM I/O*/

static int load_exam_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen exam file");
        return -1;
    }
    char buf[64];
    if (!fgets(buf, sizeof(buf), f)) {
        fprintf(stderr, "Exam file %s is empty\n", filename);
        fclose(f);
        return -1;
    }
    fclose(f);
    int num = atoi(buf);
    return num;
}

static void load_exam_into_shared(shared_t *sh, int exam_index) {
    if (exam_index < 0 || exam_index >= num_exams) {
        fprintf(stderr, "Invalid exam index %d\n", exam_index);
        return;
    }
    int student = load_exam_file(exam_files[exam_index]);
    if (student < 0) {
        fprintf(stderr, "Failed to load exam %s\n", exam_files[exam_index]);
        return;
    }
    sh->current_exam_index = exam_index;
    sh->student_number = student;
    for (int i = 0; i < NUM_Q; ++i) {
        sh->question_state[i] = Q_NOT_MARKED;
    }
}

/*LOGGING HELPERS*/

static const char *qstate_name(qstate_t s) {
    switch (s) {
        case Q_NOT_MARKED:  return "NOT_MARKED";
        case Q_IN_PROGRESS: return "IN_PROGRESS";
        case Q_DONE:        return "DONE";
        default:            return "UNKNOWN";
    }
}

/*TA LOGIC*/

static void maybe_correct_rubric_line(int id, shared_t *sh, int q_idx) {
    printf("TA %d: BEFORE READ rubric_text[%d]\n", id, q_idx);
    fflush(stdout);

    char local[32];
    snprintf(local, sizeof(local), "%s", sh->rubric_text[q_idx]);

    printf("TA %d: AFTER READ rubric_text[%d] = \"%s\"\n", id, q_idx, local);
    fflush(stdout);

    int correct = rand() % 2;  // 0 or 1

    if (!correct) {
        printf("TA %d: Decided NOT to correct rubric line %d\n", id, q_idx + 1);
        fflush(stdout);
        return;
    }

    // Modify first character after comma (skipping spaces)
    printf("TA %d: BEFORE WRITE rubric_text[%d]\n", id, q_idx);
    fflush(stdout);

    char *comma = strchr(sh->rubric_text[q_idx], ',');
    if (comma) {
        char *p = comma + 1;
        while (*p == ' ') p++;
        if (*p != '\0') {
            (*p) = (char)((unsigned char)(*p) + 1);
        }
    }

    printf("TA %d: AFTER WRITE rubric_text[%d] = \"%s\" (corrected)\n",
           id, q_idx, sh->rubric_text[q_idx]);
    fflush(stdout);

    // Save entire rubric back to file
    printf("TA %d: Saving rubric to file \"%s\"\n", id, rubric_filename);
    fflush(stdout);
    save_rubric_from_shared(sh);
}

static int pick_question(int id, shared_t *sh) {
    for (int i = 0; i < NUM_Q; ++i) {
        printf("TA %d: BEFORE READ question_state[%d]\n", id, i);
        fflush(stdout);
        qstate_t st = sh->question_state[i];
        printf("TA %d: AFTER READ question_state[%d] = %s\n",
               id, i, qstate_name(st));
        fflush(stdout);

        if (st == Q_NOT_MARKED) {
            printf("TA %d: BEFORE WRITE question_state[%d] = IN_PROGRESS\n",
                   id, i);
            fflush(stdout);
            sh->question_state[i] = Q_IN_PROGRESS;
            printf("TA %d: AFTER WRITE question_state[%d] = %s\n",
                   id, i, qstate_name(sh->question_state[i]));
            fflush(stdout);
            return i;
        }
    }
    return -1;  // none available
}

static void load_next_exam_if_any(int id, shared_t *sh) {
    printf("TA %d: BEFORE READ current_exam_index\n", id);
    fflush(stdout);
    int cur = sh->current_exam_index;
    printf("TA %d: AFTER READ current_exam_index = %d\n", id, cur);
    fflush(stdout);

    int next = cur + 1;
    if (next >= num_exams) {
        printf("TA %d: No more exam files configured (index %d). "
               "Setting terminate flag.\n", id, next);
        fflush(stdout);
        printf("TA %d: BEFORE WRITE terminate\n", id);
        fflush(stdout);
        sh->terminate = 1;
        printf("TA %d: AFTER WRITE terminate = %d\n", id, sh->terminate);
        fflush(stdout);
        return;
    }

    printf("TA %d: Loading next exam file %s (index %d)\n",
           id, exam_files[next], next);
    fflush(stdout);

    int student = load_exam_file(exam_files[next]);
    if (student < 0) {
        printf("TA %d: Failed to load next exam. Setting terminate.\n", id);
        fflush(stdout);
        sh->terminate = 1;
        return;
    }

    printf("TA %d: BEFORE WRITE current_exam_index, student_number, question_state[]\n", id);
    fflush(stdout);

    sh->current_exam_index = next;
    sh->student_number = student;
    for (int i = 0; i < NUM_Q; ++i) {
        sh->question_state[i] = Q_NOT_MARKED;
    }

    printf("TA %d: AFTER WRITE current_exam_index = %d, student_number = %04d\n",
           id, sh->current_exam_index, sh->student_number);
    for (int i = 0; i < NUM_Q; ++i) {
        printf("TA %d: question_state[%d] = %s\n", id, i,
               qstate_name(sh->question_state[i]));
    }
    fflush(stdout);

    if (student == 9999) {
        printf("TA %d: Sentinel exam (9999) reached. Setting terminate = 1\n", id);
        fflush(stdout);
        printf("TA %d: BEFORE WRITE terminate\n", id);
        fflush(stdout);
        sh->terminate = 1;
        printf("TA %d: AFTER WRITE terminate = %d\n", id, sh->terminate);
        fflush(stdout);
    }
}

static void ta_process(int id, shared_t *sh) {
    srand((unsigned int)(time(NULL) ^ getpid()));

    while (1) {
        // Check terminate flag
        printf("TA %d: BEFORE READ terminate\n", id);
        fflush(stdout);
        int term = sh->terminate;
        printf("TA %d: AFTER READ terminate = %d\n", id, term);
        fflush(stdout);
        if (term) break;

        // Read current student for logging
        printf("TA %d: BEFORE READ student_number\n", id);
        fflush(stdout);
        int student = sh->student_number;
        printf("TA %d: AFTER READ student_number = %04d\n", id, student);
        fflush(stdout);

        // If current exam file is sentinel, stop
        if (student == 9999) {
            printf("TA %d: Sentinel exam already loaded, exiting.\n", id);
            fflush(stdout);
            break;
        }

        printf("TA %d: Starting rubric pass for exam %04d\n", id, student);
        fflush(stdout);

        // Rubric pass: for each question, delay 0.5–1.0 s and call maybe_correct_rubric_line
        for (int q = 0; q < NUM_Q; ++q) {
            sleep_random(0.5, 1.0);
            maybe_correct_rubric_line(id, sh, q);
        }

        printf("TA %d: Finished rubric pass for exam %04d\n", id, student);
        fflush(stdout);

        // Mark questions for this exam
        while (1) {
            printf("TA %d: BEFORE READ terminate (inside marking loop)\n", id);
            fflush(stdout);
            int t2 = sh->terminate;
            printf("TA %d: AFTER READ terminate = %d\n", id, t2);
            fflush(stdout);
            if (t2) goto out;

            int q = pick_question(id, sh);
            if (q == -1) {
                printf("TA %d: No unmarked questions left for current exam. "
                       "Will attempt to load next exam.\n", id);
                fflush(stdout);
                load_next_exam_if_any(id, sh);
                break;  // break marking loop -> go to outer loop (next exam)
            }

            printf("TA %d: Marking exam %04d question %d ...\n",
                   id, sh->student_number, q + 1);
            fflush(stdout);

            sleep_random(1.0, 2.0);

            printf("TA %d: BEFORE WRITE question_state[%d] = DONE\n", id, q);
            fflush(stdout);
            sh->question_state[q] = Q_DONE;
            printf("TA %d: AFTER WRITE question_state[%d] = %s\n",
                   id, q, qstate_name(sh->question_state[q]));
            fflush(stdout);

            printf("TA %d: Finished marking exam %04d question %d\n",
                   id, sh->student_number, q + 1);
            fflush(stdout);
        }
    }

out:
    printf("TA %d: Terminating.\n", id);
    fflush(stdout);
    _exit(0);
}

/*MAIN*/

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_TAs>=2\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_TAs = atoi(argv[1]);
    if (num_TAs < 2) {
        fprintf(stderr, "num_TAs must be >= 2\n");
        return EXIT_FAILURE;
    }

    // Create shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(shared_t), IPC_CREAT | 0666);
    if (shmid < 0) die("shmget");

    shared_t *sh = (shared_t *)shmat(shmid, NULL, 0);
    if (sh == (void *)-1) die("shmat");

    memset(sh, 0, sizeof(*sh));

    // Initialize shared data: rubric + first exam
    load_rubric_into_shared(sh);
    load_exam_into_shared(sh, 0);

    printf("Parent: Loaded rubric and first exam %s (student %04d) "
           "into shared memory.\n",
           exam_files[0], sh->student_number);
    fflush(stdout);

    // Fork TA processes
    for (int i = 0; i < num_TAs; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        } else if (pid == 0) {
            // child
            ta_process(i, sh);
        }
    }

    // Parent: wait for all children
    for (int i = 0; i < num_TAs; ++i) {
        int status;
        wait(&status);
    }

    printf("Parent: All TA processes finished. Cleaning up shared memory.\n");
    fflush(stdout);

    // Detach and remove shared memory
    if (shmdt(sh) < 0) perror("shmdt");
    if (shmctl(shmid, IPC_RMID, NULL) < 0) perror("shmctl IPC_RMID");

    return EXIT_SUCCESS;
}

