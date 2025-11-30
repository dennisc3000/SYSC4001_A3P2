// Part 2b: Semaphore based synchronization with shared memory
// Forked processes (TAs), SysV shared memory, SysV semaphores
// All shared memory reads/writes are logged. Based on Part 2a logic

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
#include <sys/sem.h>

#define NUM_Q 5

typedef enum {
    Q_NOT_MARKED = 0,
    Q_IN_PROGRESS = 1,
    Q_DONE = 2
} qstate_t;

typedef struct {
    char rubric_text[NUM_Q][32];
    int  current_exam_index;
    int  student_number;
    qstate_t question_state[NUM_Q];
    int  terminate;
} shared_t;

/*CONFIG*/

static const char *exam_files[] = {
    "exam_files/exam01.txt", "exam_files/exam02.txt",
    "exam_files/exam03.txt", "exam_files/exam04.txt",
    "exam_files/exam05.txt", "exam_files/exam06.txt",
    "exam_files/exam07.txt", "exam_files/exam08.txt",
    "exam_files/exam09.txt", "exam_files/exam10.txt",
    "exam_files/exam11.txt", "exam_files/exam12.txt",
    "exam_files/exam13.txt", "exam_files/exam14.txt",
    "exam_files/exam15.txt", "exam_files/exam16.txt",
    "exam_files/exam17.txt", "exam_files/exam18.txt",
    "exam_files/exam19.txt", "exam_files/exam20.txt"
};
static const int num_exams = sizeof(exam_files)/sizeof(exam_files[0]);
static const char *rubric_filename = "rubric.txt";

/*UTILS*/

static double urand01(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

static void sleep_random(double min_s, double max_s) {
    double span = max_s - min_s;
    double s = min_s + urand01() * span;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/*SHARED I/O*/

static void load_rubric_into_shared(shared_t *sh) {
    FILE *f = fopen(rubric_filename, "r");
    if (!f) die("rubric open");
    for (int i = 0; i < NUM_Q; ++i) {
        char buf[128];
        fgets(buf, sizeof(buf), f);
        buf[strcspn(buf, "\r\n")] = '\0';
        snprintf(sh->rubric_text[i], sizeof(sh->rubric_text[i]), "%s", buf);
    }
    fclose(f);
}

static void save_rubric_from_shared(shared_t *sh) {
    FILE *f = fopen(rubric_filename, "w");
    if (!f) { perror("rubric write"); return; }
    for (int i = 0; i < NUM_Q; ++i) {
        fprintf(f, "%s\n", sh->rubric_text[i]);
    }
    fclose(f);
}

static int load_exam_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    char buf[64];
    fgets(buf, sizeof(buf), f);
    fclose(f);
    return atoi(buf);
}

static void load_exam_into_shared(shared_t *sh, int index) {
    int student = load_exam_file(exam_files[index]);
    sh->current_exam_index = index;
    sh->student_number = student;
    for (int i = 0; i < NUM_Q; ++i)
        sh->question_state[i] = Q_NOT_MARKED;
}

/*SEMAPHORES*/

enum {
    SEM_RUBRIC = 0,
    SEM_EXAMLOAD = 1,
    SEM_QUESTIONS = 2,
    SEM_COUNT = 3
};

static int semid;

static void P(int sem) {
    struct sembuf op = { sem, -1, 0 };
    semop(semid, &op, 1);
}

static void V(int sem) {
    struct sembuf op = { sem, +1, 0 };
    semop(semid, &op, 1);
}

/*LOGGING*/

static const char *qstate_name(qstate_t s) {
    switch (s) {
        case Q_NOT_MARKED: return "NOT_MARKED";
        case Q_IN_PROGRESS: return "IN_PROGRESS";
        case Q_DONE: return "DONE";
        default: return "UNKNOWN";
    }
}

/*TA LOGIC*/

static void maybe_correct_rubric_line(int id, shared_t *sh, int q) {

    printf("TA %d: BEFORE READ rubric_text[%d]\n", id, q);
    fflush(stdout);
    char local[32];
    snprintf(local, sizeof(local), "%s", sh->rubric_text[q]);
    printf("TA %d: AFTER READ rubric_text[%d] = \"%s\"\n", id, q, local);
    fflush(stdout);

    if (!(rand() % 2)) {
        printf("TA %d: No correction for rubric line %d\n", id, q+1);
        fflush(stdout);
        return;
    }

    printf("TA %d: BEFORE WRITE rubric_text[%d]\n", id, q);
    fflush(stdout);
    char *comma = strchr(sh->rubric_text[q], ',');
    if (comma) {
        char *p = comma + 1;
        while (*p == ' ') p++;
        if (*p) *p = *p + 1;
    }
    printf("TA %d: AFTER WRITE rubric_text[%d] = \"%s\"\n",
           id, q, sh->rubric_text[q]);
    fflush(stdout);

    printf("TA %d: Saving rubric...\n", id);
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
            printf("TA %d: BEFORE WRITE question_state[%d] = IN_PROGRESS\n", id, i);
            fflush(stdout);
            sh->question_state[i] = Q_IN_PROGRESS;
            printf("TA %d: AFTER WRITE question_state[%d] = IN_PROGRESS\n", id, i);
            fflush(stdout);
            return i;
        }
    }
    return -1;
}

static void load_next_exam_if_any(int id, shared_t *sh) {

    printf("TA %d: BEFORE READ current_exam_index\n", id);
    fflush(stdout);
    int cur = sh->current_exam_index;
    printf("TA %d: AFTER READ current_exam_index = %d\n", id, cur);
    fflush(stdout);

    int next = cur + 1;
    if (next >= num_exams) {
        printf("TA %d: No more exams. Setting terminate.\n", id);
        fflush(stdout);
        sh->terminate = 1;
        return;
    }

    printf("TA %d: Loading exam %s (index %d)\n",
           id, exam_files[next], next);
    fflush(stdout);

    int num = load_exam_file(exam_files[next]);

    printf("TA %d: BEFORE WRITE exam fields\n", id);
    fflush(stdout);
    sh->current_exam_index = next;
    sh->student_number = num;
    for (int i = 0; i < NUM_Q; ++i)
        sh->question_state[i] = Q_NOT_MARKED;

    printf("TA %d: AFTER WRITE: exam index=%d student=%04d\n",
           id, next, num);
    fflush(stdout);

    if (num == 9999) {
        printf("TA %d: Sentinel exam reached. Setting terminate.\n", id);
        fflush(stdout);
        sh->terminate = 1;
    }
}

static void ta_process(int id, shared_t *sh) {
    srand((unsigned)(time(NULL) ^ getpid()));

    while (1) {

        printf("TA %d: BEFORE READ terminate\n", id);
        fflush(stdout);
        if (sh->terminate) break;
        printf("TA %d: AFTER READ terminate = %d\n", id, sh->terminate);
        fflush(stdout);

        /*RUBRIC PASS (protected with SEM_RUBRIC)*/

        P(SEM_RUBRIC);
        printf("TA %d: Starting rubric pass.\n", id);
        fflush(stdout);

        for (int q = 0; q < NUM_Q; ++q) {
            sleep_random(0.5, 1.0);
            maybe_correct_rubric_line(id, sh, q);
        }

        printf("TA %d: Finished rubric pass.\n", id);
        fflush(stdout);
        V(SEM_RUBRIC);

        /*MARK QUESTIONS (protected with SEM_QUESTIONS)*/

        while (1) {
            printf("TA %d: BEFORE READ terminate\n", id);
            fflush(stdout);
            if (sh->terminate) goto end;
            printf("TA %d: AFTER READ terminate = %d\n", id, sh->terminate);
            fflush(stdout);

            P(SEM_QUESTIONS);
            int q = pick_question(id, sh);
            V(SEM_QUESTIONS);

            if (q == -1) {
                /* No more questions so load next exam */
                P(SEM_EXAMLOAD);
                load_next_exam_if_any(id, sh);
                V(SEM_EXAMLOAD);
                break;
            }

            printf("TA %d: Marking exam %04d Q%d...\n",
                   id, sh->student_number, q+1);
            fflush(stdout);

            sleep_random(1.0, 2.0);

            P(SEM_QUESTIONS);
            printf("TA %d: BEFORE WRITE question_state[%d] = DONE\n", id, q);
            fflush(stdout);
            sh->question_state[q] = Q_DONE;
            printf("TA %d: AFTER WRITE question_state[%d] = DONE\n", id, q);
            fflush(stdout);
            V(SEM_QUESTIONS);

            printf("TA %d: Finished marking exam %04d Q%d\n",
                   id, sh->student_number, q+1);
            fflush(stdout);
        }
    }

end:
    printf("TA %d: Terminating.\n", id);
    fflush(stdout);
    _exit(0);
}

/*MAIN*/

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_TAs>=2\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]);
    if (n < 2) {
        fprintf(stderr, "num_TAs must be >= 2\n");
        return 1;
    }

    /* Shared memory */
    int shmid = shmget(IPC_PRIVATE, sizeof(shared_t), IPC_CREAT | 0666);
    if (shmid < 0) die("shmget");

    shared_t *sh = shmat(shmid, NULL, 0);
    if (sh == (void *)-1) die("shmat");

    memset(sh, 0, sizeof(*sh));

    load_rubric_into_shared(sh);
    load_exam_into_shared(sh, 0);

    /* Semaphores */
    semid = semget(IPC_PRIVATE, SEM_COUNT, IPC_CREAT | 0666);
    if (semid < 0) die("semget");

    semctl(semid, SEM_RUBRIC,   SETVAL, 1);
    semctl(semid, SEM_EXAMLOAD, SETVAL, 1);
    semctl(semid, SEM_QUESTIONS,SETVAL, 1);

    printf("Parent: Initialized shared memory + semaphores.\n");
    fflush(stdout);

    /* Fork TAs */
    for (int i = 0; i < n; ++i) {
        if (fork() == 0)
            ta_process(i, sh);
    }

    for (int i = 0; i < n; ++i)
        wait(NULL);

    printf("Parent: All TAs terminated. Cleaning up.\n");
    fflush(stdout);

    shmdt(sh);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    return 0;
}
