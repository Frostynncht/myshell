#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobState;

// подготавливает и освобождает таблицу заданий
void init_jobs(void);
void free_jobs(void);

// добавляет задание в таблицу и возвращает его номер
int add_job(pid_t pgid, const pid_t *pids, int pid_count, JobState state, const char *command);

// проверяет есть ли остановленные задания
int have_stopped_jobs(void);

#endif