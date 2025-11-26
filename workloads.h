#ifndef WORKLOADS_H
#define WORKLOADS_H
#include "task_scheduler.h"

typedef struct {
   double** A;
   double** B;
   double** C;
   int row;
   int size;
} MatrixTask;

void matrix_row_task(void* arg);
void run_matrix_workload(TaskScheduler* sched, int size);

typedef struct {
   int* array;
   int start;
   int end;
   long* result;
} ReductionTask;

void reduction_task(void* arg);
void run_reduction_workload(TaskScheduler* sched, int array_size);

void light_task(void* arg);
void medium_task(void* arg);
void heavy_task(void* arg);
void run_mixed_workload(TaskScheduler* sched, int num_tasks);


#endif
