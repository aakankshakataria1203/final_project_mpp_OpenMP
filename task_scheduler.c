#include "task_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


static uint64_t get_time_ns() {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}


void scheduler_init(TaskScheduler* sched, int num_threads, int capacity, ScheduleMode mode) {
   sched->task_queue = malloc(sizeof(Task) * capacity);
   sched->capacity = capacity;
   sched->head = 0;
   sched->tail = 0;
   sched->active_tasks = 0;
   sched->num_threads = num_threads;
   sched->mode = mode;
   sched->running = false;

   sched->metrics.tasks_completed = 0;
   sched->metrics.total_exec_time_ns = 0;
   sched->metrics.idle_time_ns = 0;
   sched->metrics.queue_accesses = 0;
  
   omp_set_num_threads(num_threads);
}


void scheduler_submit(TaskScheduler* sched, void (*func)(void*), void* arg, TaskWeight weight) {
   int tail = sched->tail;
   sched->task_queue[tail % sched->capacity].func = func;
   sched->task_queue[tail % sched->capacity].arg = arg;
   sched->task_queue[tail % sched->capacity].weight = weight;
   sched->task_queue[tail % sched->capacity].id = tail;
   sched->tail++;
   sched->active_tasks++;
}


static void execute_task_static(TaskScheduler* sched) {
   int total = sched->tail;
  
   #pragma omp parallel
   {
       int tid = omp_get_thread_num();
       int nthreads = omp_get_num_threads();
       int chunk_size = (total + nthreads - 1) / nthreads;
       int start = tid * chunk_size;
       int end = (start + chunk_size > total) ? total : start + chunk_size;
      
       for (int i = start; i < end; i++) {
           uint64_t t_start = get_time_ns();
           sched->task_queue[i].func(sched->task_queue[i].arg);
           uint64_t t_end = get_time_ns();
          
           #pragma omp atomic
           sched->metrics.total_exec_time_ns += (t_end - t_start);
          
           #pragma omp atomic
           sched->metrics.tasks_completed++;
          
           #pragma omp atomic
           sched->active_tasks--;
       }
   }
}


static void execute_task_dynamic(TaskScheduler* sched) {
   int total = sched->tail;
  
   #pragma omp parallel for schedule(dynamic, 1)
   for (int i = 0; i < total; i++) {
       uint64_t t_start = get_time_ns();
       sched->task_queue[i].func(sched->task_queue[i].arg);
       uint64_t t_end = get_time_ns();
      
       #pragma omp atomic
       sched->metrics.total_exec_time_ns += (t_end - t_start);
      
       #pragma omp atomic
       sched->metrics.tasks_completed++;
      
       #pragma omp atomic
       sched->active_tasks--;
   }
}


static void execute_task_guided(TaskScheduler* sched) {
   int total = sched->tail;
  
   #pragma omp parallel for schedule(guided)
   for (int i = 0; i < total; i++) {
       uint64_t t_start = get_time_ns();
       sched->task_queue[i].func(sched->task_queue[i].arg);
       uint64_t t_end = get_time_ns();
      
       #pragma omp atomic
       sched->metrics.total_exec_time_ns += (t_end - t_start);
      
       #pragma omp atomic
       sched->metrics.tasks_completed++;
      
       #pragma omp atomic
       sched->active_tasks--;
   }
}


static void execute_task_heterogeneous(TaskScheduler* sched) {
   int total = sched->tail;
  
   Task* sorted = malloc(sizeof(Task) * total);
   int light_count = 0, medium_count = 0, heavy_count = 0;
  
   for (int i = 0; i < total; i++) {
       if (sched->task_queue[i].weight == TASK_LIGHT) light_count++;
       else if (sched->task_queue[i].weight == TASK_MEDIUM) medium_count++;
       else heavy_count++;
   }
  
   int light_idx = 0, medium_idx = light_count, heavy_idx = light_count + medium_count;
  
   for (int i = 0; i < total; i++) {
       if (sched->task_queue[i].weight == TASK_LIGHT)
           sorted[light_idx++] = sched->task_queue[i];
       else if (sched->task_queue[i].weight == TASK_MEDIUM)
           sorted[medium_idx++] = sched->task_queue[i];
       else
           sorted[heavy_idx++] = sched->task_queue[i];
   }
  
   #pragma omp parallel
   {
       int tid = omp_get_thread_num();
       int nthreads = omp_get_num_threads();
      
       int light_chunk = (light_count + nthreads - 1) / nthreads;
       int start = tid * light_chunk;
       int end = (start + light_chunk > light_count) ? light_count : start + light_chunk;
      
       for (int i = start; i < end; i++) {
           uint64_t t_start = get_time_ns();
           sorted[i].func(sorted[i].arg);
           uint64_t t_end = get_time_ns();
          
           #pragma omp atomic
           sched->metrics.total_exec_time_ns += (t_end - t_start);
          
           #pragma omp atomic
           sched->metrics.tasks_completed++;
          
           #pragma omp atomic
           sched->active_tasks--;
       }
      
       #pragma omp for schedule(dynamic, 1)
       for (int i = light_count; i < total; i++) {
           uint64_t t_start = get_time_ns();
           sorted[i].func(sorted[i].arg);
           uint64_t t_end = get_time_ns();
          
           #pragma omp atomic
           sched->metrics.total_exec_time_ns += (t_end - t_start);
          
           #pragma omp atomic
           sched->metrics.tasks_completed++;
          
           #pragma omp atomic
           sched->active_tasks--;
       }
   }
  
   free(sorted);
}


void scheduler_run(TaskScheduler* sched) {
   sched->running = true;
  
   switch (sched->mode) {
       case SCHEDULE_STATIC:
           execute_task_static(sched);
           break;
       case SCHEDULE_DYNAMIC:
           execute_task_dynamic(sched);
           break;
       case SCHEDULE_GUIDED:
           execute_task_guided(sched);
           break;
       case SCHEDULE_HETEROGENEOUS:
           execute_task_heterogeneous(sched);
           break;
       case SCHEDULE_ADAPTIVE:
           execute_task_heterogeneous(sched);
           break;
   }
  
   sched->running = false;
}


void scheduler_wait(TaskScheduler* sched) {
   while (sched->active_tasks > 0) {
       usleep(100);
   }
}


void scheduler_destroy(TaskScheduler* sched) {
   free(sched->task_queue);
}


void scheduler_print_metrics(TaskScheduler* sched) {
   uint64_t completed = sched->metrics.tasks_completed;
   uint64_t total_time = sched->metrics.total_exec_time_ns;
  
   printf("Tasks Completed: %lu\n", completed);
   printf("Total Execution Time: %.2f ms\n", total_time / 1e6);
   printf("Avg Task Time: %.3f ms\n", completed > 0 ? (total_time / 1e6) / completed : 0);
}


double scheduler_get_throughput(TaskScheduler* sched, double duration_sec) {
   uint64_t completed = sched->metrics.tasks_completed;
   return completed / duration_sec;
}


double scheduler_get_efficiency(TaskScheduler* sched) {
   uint64_t total_time = sched->metrics.total_exec_time_ns;
   uint64_t idle_time = sched->metrics.idle_time_ns;
  
   if (total_time + idle_time == 0) return 0.0;
   return (double)total_time / (total_time + idle_time);
}
