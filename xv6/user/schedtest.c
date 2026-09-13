#include "kernel/types.h" 
#include "kernel/stat.h" 
#include "user/user.h" 

#define NPROCS 4 // num of procs to run

void cpu_bound_work(void) { // func for heavy cpu work
  for (volatile int i=0; i<200000000; i++) { // volatile so it dsnt optimize process
    // Heavy CPU computation to check how os handles scheduler refusing to share cpu time
  } 
}

void io_bound_work(void) { // fake io wait
  for (int i=0; i<20; i++) { // loop a bit
    pause(1); // Voluntary yield / sleep, i.e. giv up cpu to sleep
  } 
} 

int main(int argc, char *argv[]) { 
  printf("Scheduler Benchmark Test (NPROCS=%d)\n", NPROCS); // print start msg

  for (int i=0; i<NPROCS; i++) { 
    int pid = fork(); // make a child proc
    if (pid==0) { // if its the kid
      if (i%2==0) { 
        cpu_bound_work();  // even children do heavy comp
      } else { 
        io_bound_work(); // odd children do io and sleeping
      } 
      exit(0); 
    }
  } 

  int total_rtime=0; // track total run time
  int total_wtime=0; // track total wait time
  int total_ttime=0; // track total turnaroud
  int total_restime=0; // track total response time

  for (int i=0; i<NPROCS; i++) 
  { // loop to wait for child proc
    int r, w, res; // added res var
    
    // u will need to update waitx in ur kernel to accept this 4th pointer!
    int pid = waitx(0, &r, &w, &res); 
    
    int turnaround = r+w; // calc total time from start to fin

    total_rtime += r; // add run to totl
    total_wtime += w; // add wait to totl
    total_ttime += turnaround; // add turn to totl
    total_restime += res; //  add response time to totl

    printf("Process PID %d: RunTime = %d, WaitTime = %d, TurnaroundTime = %d, ResponseTime = %d\n", 
           pid, r, w, turnaround, res); //print res
  }

  printf("\nSummary Statistics\n");
  printf("Average Running Time: %d ticks\n", total_rtime / NPROCS); // avg run time (fixed string)
  printf("Average Waiting Time: %d ticks\n", total_wtime / NPROCS); // avg wait time
  printf("Average Turnaround Time: %d ticks\n", total_ttime / NPROCS); // avg turnaroud
  printf("Average Response Time: %d ticks\n", total_restime / NPROCS); //avg response time

  exit(0); 
}