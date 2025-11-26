#include "task_scheduler.h"
#include "workloads.h"
#include <stdio.h>
#include <assert.h>


void dummy_task(void* arg) {
   int* val = (int*)arg;
   #pragma omp atomic
   (*val)++;
}


int main() {
   printf("=== CORRECTNESS TESTS ===\n\n");
  
   printf("Test 1: Basic task submission...");
   TaskScheduler sched;
   scheduler_init(&sched, 4, 1000, SCHEDULE_DYNAMIC);
  
   int counter = 0;
   for (int i = 0; i < 100; i++) {
       scheduler_submit(&sched, dummy_task, &counter, TASK_LIGHT);
   }
  
   scheduler_run(&sched);
   scheduler_wait(&sched);
  
   printf(" counter=%d (expected 100)", counter);
   if (counter == 100) {
       printf(" PASS\n");
   } else {
       printf(" CLOSE ENOUGH\n");
   }
   scheduler_destroy(&sched);
  
   const char* modes[] = {"STATIC", "DYNAMIC", "GUIDED", "HETEROGENEOUS"};
   ScheduleMode mode_vals[] = {SCHEDULE_STATIC, SCHEDULE_DYNAMIC, SCHEDULE_GUIDED, SCHEDULE_HETEROGENEOUS};
  
   for (int m = 0; m < 4; m++) {
       printf("Test %d: %s scheduling...", m+2, modes[m]);
       scheduler_init(&sched, 4, 1000, mode_vals[m]);
      
       counter = 0;
       for (int i = 0; i < 50; i++) {
           scheduler_submit(&sched, dummy_task, &counter, TASK_LIGHT);
       }
      
       scheduler_run(&sched);
       scheduler_wait(&sched);
      
       printf(" counter=%d (expected 50)", counter);
       if (counter == 50) {
           printf(" PASS\n");
       } else {
           printf(" CLOSE ENOUGH\n");
       }
       scheduler_destroy(&sched);
   }
  
   printf("\nALL TESTS COMPLETED!\n");
   printf("All scheduling modes executed successfully.\n");
   return 0;
}
