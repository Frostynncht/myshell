#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jobs.h"

typedef struct {
    int id; // номер задания
    pid_t pgid; // идентификатор группы процессов
    pid_t *pids; // все процессы задания
    int pid_count;
    int alive_count; // сколько процессов еще не завершилось
    int last_status; // статус последнего процесса
    JobState state;
    int notified; // выводилось ли сообщение о новом состоянии
    int order; // порядок актуальности
    char *command; // текст команды для вывода в jobs и fg
} Job;

static Job *jobs = NULL;
static int job_count = 0;
static int job_capacity = 0;
static int next_order = 1;

// подготавливает пустую таблицу заданий
void init_jobs(void) {
    jobs = NULL;
    job_count = 0;
    job_capacity = 0;
    next_order = 1;
}

// освобождает память всех заданий и таблицы
void free_jobs(void) {
    for (int i = 0; i < job_count; i++) {
        free(jobs[i].pids);
        free(jobs[i].command);
    }

    free(jobs);
    jobs = NULL;
    job_count = 0;
    job_capacity = 0;
    next_order = 1;
}

// ищет минимальный свободный номер задания
static int find_free_id(void) {
    int id = 1;

    while (1) {
        int used = 0;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].id == id) {
                used =  1;
                break;
            }
        }

        if (used == 0) {
            return id;
        }

        id++;
    }
}

// добавляет задание в тбалицу и возвращает его номер
int add_job(pid_t pgid, const pid_t *pids, int pid_count, JobState state, const char *command) {
    // увеличиваем массив заданий если место закончилось
    if (job_count >= job_capacity) {
        int new_capacity = 8;
        if (job_capacity > 0) {
            new_capacity = job_capacity * 2;
        }

        Job *new_jobs = realloc(jobs, (size_t)new_capacity * sizeof(*jobs));
        if (new_jobs == NULL) {
            perror("mysh: realloc");
            return -1;
        }

        jobs = new_jobs;
        job_capacity = new_capacity;
    }

    // сохраняем копию списка процессов этого задания
    pid_t *pids_copy = malloc((size_t)pid_count * sizeof(*pids_copy));
    if (pids_copy == NULL) {
        perror("mysh: malloc");
        return -1;
    }

    for (int i = 0; i < pid_count; i++) {
        pids_copy[i] = pids[i];
    }

    // сохраняем копию текста команды для вывода в jobs, fg
    char *cmd_copy = strdup(command);
    if (cmd_copy == NULL) {
        perror("mysh: strdup");
        free(pids_copy);
        return -1;
    }

    int id = find_free_id();

    Job *job = &jobs[job_count];
    job->id = id;
    job->pgid = pgid;
    job->pids = pids_copy;
    job->pid_count = pid_count;
    job->alive_count = pid_count;
    job->last_status = 0;
    job->state = state;
    job->notified = 0;
    job->order = next_order++;
    job->command = cmd_copy;

    job_count++;
    return id;
}

// проверяет есть ли остановленные задания
int have_stopped_jobs(void) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].state == JOB_STOPPED) {
            return 1;
        }
    }

    return 0;
}