#include "task_scheduler.h"
#include "workloads.h"
#include <stdio.h>
#include <time.h>
#include <omp.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_THREADS 32
#define MAX_TASKS   10000

uint64_t get_time_ns() {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ((uint64_t) ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

double get_time_sec() {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}

double benchmark_lock_based(int num_threads, int num_tasks,
                            int *thread_counts, double *task_latencies) {
    omp_set_num_threads(num_threads);
    
    int *task_types = malloc(num_tasks * sizeof(int));
    for (int i = 0; i < num_tasks; i++) {
        task_types[i] = i % 3;
    }
    
    omp_lock_t task_lock;
    omp_init_lock(&task_lock);
    
    int next_task_idx = 0;
    int total_tasks = num_tasks;
    
    double start = get_time_sec();
    
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        
        while (1) {
            int my_task = -1;
            
            omp_set_lock(&task_lock);
            if (next_task_idx < total_tasks) {
                my_task = next_task_idx;
                next_task_idx++;
            }
            omp_unset_lock(&task_lock);
            
            if (my_task == -1) {
                break;
            }
            
            uint64_t t_start = get_time_ns();
            
            int task_type = task_types[my_task];
            
            if (task_type == 0) {
                // LIGHT task: 1000 iterations
                volatile int sum = 0;
                for (int i = 0; i < 1000; i++) {
                    sum += i;
                }
            } 
            else if (task_type == 1) {
                // MEDIUM task: 10000 iterations with sqrt
                volatile double sum = 0.0;
                for (int i = 0; i < 10000; i++) {
                    sum += sqrt((double)i);
                }
            } 
            else {
                // HEAVY task: 100000 iterations with sin/cos
                volatile double sum = 0.0;
                for (int i = 0; i < 100000; i++) {
                    sum += sin((double)i) * cos((double)i);
                }
            }
            
            uint64_t t_end = get_time_ns();
            task_latencies[my_task] = (t_end - t_start) / 1e6;  // convert to ms
            
            #pragma omp atomic
            thread_counts[tid]++;
        }
    }
    
    double duration = get_time_sec() - start;
    
    omp_destroy_lock(&task_lock);
    free(task_types);
    
    return duration;
}

void benchmark_schedule_mode(ScheduleMode mode, int num_threads,
                           const char* workload, double* duration_out, double* throughput_out,
                           int *thread_counts, double *task_latencies, int ntasks) {
   TaskScheduler sched;
   scheduler_init(&sched, num_threads, ntasks, mode);
   if (strcmp(workload, "mixed") == 0) run_mixed_workload(&sched, ntasks);
   else if (strcmp(workload, "matrix") == 0) run_matrix_workload(&sched, 50);
   else if (strcmp(workload, "reduction") == 0) run_reduction_workload(&sched, ntasks);
   
   double start = get_time_sec();
   
   #pragma omp parallel
   {
       int tid = omp_get_thread_num();
       int executed = 0;
       #pragma omp for schedule(dynamic, 1)
       for (int i = 0; i < sched.tail; i++) {
           uint64_t t_start = get_time_ns();
           sched.task_queue[i].func(sched.task_queue[i].arg);
           executed++;
           uint64_t t_end = get_time_ns();
           task_latencies[i] = (t_end - t_start) / 1e6; // ms
       }
       thread_counts[tid] = executed;
   }
   
   double duration = get_time_sec() - start;
   double throughput = (ntasks) / duration;
   *duration_out = duration;
   *throughput_out = throughput;
   scheduler_destroy(&sched);
}

void print_csv_table_header() {
   printf("Workload,Mode,Threads,Duration_sec,Throughput,Speedup,Efficiency\n");
}

void print_csv_table_row(const char* workload, const char* mode, int threads,
                       double duration, double throughput, double speedup, double efficiency) {
   printf("%s,%s,%d,%.5f,%.2f,%.3f,%.2f\n", workload, mode, threads, duration, throughput, speedup, efficiency);
}

void print_fairness_header() {
   printf("Mode,Threads,MinTasks,MaxTasks,MeanTasks,SD_Tasks,Fairness\n");
}

void print_fairness_row(const char* mode, int threads, int min, int max, double mean, double sd, double fairness) {
   printf("%s,%d,%d,%d,%.2f,%.2f,%.2f\n", mode, threads, min, max, mean, sd, fairness);
}

void analyze_thread_load(int threads, int *thread_counts, int* min, int* max, double* mean, double* sd, double* fairness) {
   *min = thread_counts[0];
   *max = thread_counts[0];
   int sum = 0;
   
   for (int i = 0; i < threads; i++) {
       if (thread_counts[i] < *min) *min = thread_counts[i];
       if (thread_counts[i] > *max) *max = thread_counts[i];
       sum += thread_counts[i];
   }
   
   *mean = (double)sum / (double)threads;
   
   double stdev = 0.0;
   for (int i = 0; i < threads; i++)
       stdev += (thread_counts[i] - *mean) * (thread_counts[i] - *mean);
   *sd = sqrt(stdev / threads);
   
   *fairness = (*mean > 0) ? 100.0 * (*min) / (*mean) : 0.0;
}

void print_histogram_header() {
   printf("Bin_0_1ms,Bin_1_2ms,Bin_2_5ms,Bin_5_10ms,Bin_10_20ms,Bin_20_50ms,Bin_50_100ms,Bin_100pms\n");
}

void print_latency_histogram(double *latencies, int ntasks) {
   int buckets[8] = {0};
   
   for (int i = 0; i < ntasks; i++) {
       double l = latencies[i];
       if (l < 1) buckets[0]++;
       else if (l < 2) buckets[1]++;
       else if (l < 5) buckets[2]++;
       else if (l < 10) buckets[3]++;
       else if (l < 20) buckets[4]++;
       else if (l < 50) buckets[5]++;
       else if (l < 100) buckets[6]++;
       else buckets[7]++;
   }
   
   for (int i = 0; i < 8; i++) printf("%d%s", buckets[i], (i < 7) ? "," : "\n");
}

// STRESS TEST: 100,000 tiny tasks
void run_stress_test_comparison() {
    printf("\n=== STRESS TEST (Fine-Grained 100k tasks) ===\n");
    printf("Mode,Threads,Duration_sec,Throughput\n");

    int ntasks = 100000;
    int T = 16;
    
    double start = get_time_sec();
    omp_set_num_threads(T);
    omp_lock_t lock;
    omp_init_lock(&lock);
    int next = 0;
    #pragma omp parallel
    {
        while(1) {
            int my_task = -1;
            omp_set_lock(&lock);
            if (next < ntasks) my_task = next++;
            omp_unset_lock(&lock);
            if (my_task == -1) break;
            volatile int x = 0; x++;
        }
    }
    double duration_lock = get_time_sec() - start;
    printf("LOCK_BASED,%d,%.5f,%.2f\n", T, duration_lock, ntasks/duration_lock);
    omp_destroy_lock(&lock);

    start = get_time_sec();
    #pragma omp parallel for schedule(guided) num_threads(T)
    for (int i = 0; i < ntasks; i++) {
         volatile int x = 0; x++;
    }
    double duration_lf = get_time_sec() - start;
    printf("OPENMP_GUIDED,%d,%.5f,%.2f\n", T, duration_lf, ntasks/duration_lf);
    
    printf(">> Conclusion: Lock-Free is %.2fx faster on fine-grained tasks.\n", duration_lock / duration_lf);
}

int main() {
   const char* workloads[] = {"mixed"};
   const char* modes[] = {"LOCK_BASED", "STATIC", "DYNAMIC", "GUIDED", "HETEROGENEOUS"};
   ScheduleMode mode_vals[] = {SCHEDULE_STATIC, SCHEDULE_DYNAMIC, SCHEDULE_GUIDED, SCHEDULE_HETEROGENEOUS};
   int thread_counts_list[] = {1, 2, 4, 8, 12, 16};
   int num_tcs = 6, ntasks = 1000;

   double durations[5][MAX_THREADS] = {{0}};
   double throughputs[5][MAX_THREADS] = {{0}};
   
   int fairness_min[5][MAX_THREADS] = {{0}};
   int fairness_max[5][MAX_THREADS] = {{0}};
   double fairness_mean[5][MAX_THREADS] = {{0}};
   double fairness_sd[5][MAX_THREADS] = {{0}};
   double fairness_ratio[5][MAX_THREADS] = {{0}};

   printf("=== MIXED_WORKLOAD_RESULTS ===\n");
   print_csv_table_header();
   
   double static_1thread_duration = 0.0;

   for (int t = 0; t < num_tcs; t++) {
       int T = thread_counts_list[t];

       int lock_thread_counts[MAX_THREADS] = {0};
       double lock_latencies[MAX_TASKS] = {0.0};
       double lock_duration = benchmark_lock_based(T, ntasks, lock_thread_counts, lock_latencies);
       double lock_throughput = ntasks / lock_duration;
       durations[0][t] = lock_duration;
       throughputs[0][t] = lock_throughput;
       
       int min, max;
       double mean, sd, fairness;
       analyze_thread_load(T, lock_thread_counts, &min, &max, &mean, &sd, &fairness);
       fairness_min[0][t] = min;
       fairness_max[0][t] = max;
       fairness_mean[0][t] = mean;
       fairness_sd[0][t] = sd;
       fairness_ratio[0][t] = fairness;

       for (int m = 0; m < 4; m++) {
           int thread_stats[MAX_THREADS] = {0};
           double latency_stats[MAX_TASKS] = {0.0};
           double duration, throughput;
           
           benchmark_schedule_mode(mode_vals[m], T, workloads[0], &duration, &throughput, thread_stats, latency_stats, ntasks);
           
           durations[m+1][t] = duration;
           throughputs[m+1][t] = throughput;
           
           analyze_thread_load(T, thread_stats, &min, &max, &mean, &sd, &fairness);
           fairness_min[m+1][t] = min;
           fairness_max[m+1][t] = max;
           fairness_mean[m+1][t] = mean;
           fairness_sd[m+1][t] = sd;
           fairness_ratio[m+1][t] = fairness;
           
           if (T == 1 && m == 0) {
               static_1thread_duration = duration;
           }
       }
   }
   
   for (int t = 0; t < num_tcs; t++) {
       int T = thread_counts_list[t];
       for (int m = 0; m < 5; m++) {
           double speedup = static_1thread_duration / durations[m][t];
           double efficiency = (speedup / T) * 100.0;
           print_csv_table_row("mixed", modes[m], T, durations[m][t], throughputs[m][t], speedup, efficiency);
       }
   }

   printf("=== PER_THREAD_FAIRNESS ===\n");
   print_fairness_header();
   for (int m = 0; m < 5; m++) {
       for (int t = 0; t < num_tcs; t++) {
           print_fairness_row(modes[m], thread_counts_list[t],
               fairness_min[m][t], fairness_max[m][t], fairness_mean[m][t], fairness_sd[m][t], fairness_ratio[m][t]);
       }
   }

   printf("=== TASK_LATENCY_HISTOGRAM ===\n");
   print_histogram_header();
   
   {
       int lock_thread_counts[MAX_THREADS] = {0};
       double lock_latencies[MAX_TASKS] = {0.0};
       benchmark_lock_based(1, ntasks, lock_thread_counts, lock_latencies);
       print_latency_histogram(lock_latencies, ntasks);
   }

   printf("=== STRESS_TEST ===\n");
   run_stress_test_comparison();
   
   return 0;
}
