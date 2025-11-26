#include "workloads.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

void matrix_row_task(void* arg) {
   MatrixTask* task = (MatrixTask*)arg;
   for (int j = 0; j < task->size; j++) {
       double sum = 0.0;
       for (int k = 0; k < task->size; k++) {
           sum += task->A[task->row][k] * task->B[k][j];
       }
       task->C[task->row][j] = sum;
   }
   free(task);
}

void run_matrix_workload(TaskScheduler* sched, int size) {
   double** A = malloc(sizeof(double*) * size);
   double** B = malloc(sizeof(double*) * size);
   double** C = malloc(sizeof(double*) * size);
  
   for (int i = 0; i < size; i++) {
       A[i] = malloc(sizeof(double) * size);
       B[i] = malloc(sizeof(double) * size);
       C[i] = malloc(sizeof(double) * size);
       for (int j = 0; j < size; j++) {
           A[i][j] = 1.5;
           B[i][j] = 2.0;
           C[i][j] = 0.0;
       }
   }
  
   for (int i = 0; i < size; i++) {
       MatrixTask* task = malloc(sizeof(MatrixTask));
       task->A = A;
       task->B = B;
       task->C = C;
       task->row = i;
       task->size = size;
       scheduler_submit(sched, matrix_row_task, task, TASK_HEAVY);
   }
}

void reduction_task(void* arg) {
   ReductionTask* task = (ReductionTask*)arg;
   long sum = 0;
   for (int i = task->start; i < task->end; i++) {
       sum += task->array[i];
   }
   #pragma omp atomic
   *(task->result) += sum;
   free(task);
}

void run_reduction_workload(TaskScheduler* sched, int array_size) {
   int* array = malloc(sizeof(int) * array_size);
   for (int i = 0; i < array_size; i++) array[i] = 1;
  
   long* result = calloc(1, sizeof(long));
   int chunk = array_size / 100;
  
   for (int i = 0; i < array_size; i += chunk) {
       ReductionTask* task = malloc(sizeof(ReductionTask));
       task->array = array;
       task->start = i;
       task->end = (i + chunk > array_size) ? array_size : i + chunk;
       task->result = result;
       scheduler_submit(sched, reduction_task, task, TASK_LIGHT);
   }
}

void light_task(void* arg) {
   volatile int sum = 0;
   for (int i = 0; i < 1000; i++) sum += i;
   free(arg);
}


void medium_task(void* arg) {
   volatile double sum = 0.0;
   for (int i = 0; i < 10000; i++) {
       sum += sqrt((double)i);
   }
   free(arg);
}

void heavy_task(void* arg) {
   volatile double sum = 0.0;
   for (int i = 0; i < 100000; i++) {
       sum += sin((double)i) * cos((double)i);
   }
   free(arg);
}

void run_mixed_workload(TaskScheduler* sched, int num_tasks) {
   for (int i = 0; i < num_tasks; i++) {
       int* dummy = malloc(sizeof(int));
       if (i % 3 == 0) {
           scheduler_submit(sched, light_task, dummy, TASK_LIGHT);
       } else if (i % 3 == 1) {
           scheduler_submit(sched, medium_task, dummy, TASK_MEDIUM);
       } else {
           scheduler_submit(sched, heavy_task, dummy, TASK_HEAVY);
       }
   }
}
