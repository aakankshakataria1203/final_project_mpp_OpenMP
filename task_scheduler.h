#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
   TASK_LIGHT = 1,
   TASK_MEDIUM = 2,
   TASK_HEAVY = 3
} TaskWeight;

typedef struct {
   void (*func)(void*);
   void* arg;
   TaskWeight weight;
   int id;
} Task;

typedef enum {
   SCHEDULE_STATIC,
   SCHEDULE_DYNAMIC,
   SCHEDULE_GUIDED,
   SCHEDULE_HETEROGENEOUS,
   SCHEDULE_ADAPTIVE
} ScheduleMode;

typedef struct {
   uint64_t tasks_completed;
   uint64_t total_exec_time_ns;
   uint64_t idle_time_ns;
   uint64_t queue_accesses;
   double avg_exec_time_ms;
   double idle_ratio;
} RuntimeMetrics;

typedef struct {
   Task* task_queue;
   int capacity;
   int head;
   int tail;
   int active_tasks;
  
   int num_threads;
   ScheduleMode mode;
   RuntimeMetrics metrics;
  
   bool running;
   double variance_threshold;
} TaskScheduler;

void scheduler_init(TaskScheduler* sched, int num_threads, int capacity, ScheduleMode mode);
void scheduler_submit(TaskScheduler* sched, void (*func)(void*), void* arg, TaskWeight weight);
void scheduler_run(TaskScheduler* sched);
void scheduler_wait(TaskScheduler* sched);
void scheduler_destroy(TaskScheduler* sched);

void scheduler_print_metrics(TaskScheduler* sched);
double scheduler_get_throughput(TaskScheduler* sched, double duration_sec);
double scheduler_get_efficiency(TaskScheduler* sched);


#endif
