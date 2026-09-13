#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 4

void cpu_bound_work(void) {
  for (volatile int i=0; i<200000000; i++) {
    // Heavy CPU computation
  }
}

void io_bound_work(void) {
  for (int i=0; i<20; i++) {
    pause(1); // Voluntary yield / sleep
  }
}

int main(int argc, char *argv[]) {
  printf("Scheduler Benchmark Test (NPROCS=%d)\n", NPROCS);

  for (int i=0; i<NPROCS; i++) {
    int pid = fork();
    if (pid==0) {
      if (i%2==0) {
        cpu_bound_work();
      } else {
        io_bound_work();
      }
      exit(0);
    }
  }

  int total_rtime=0;
  int total_wtime=0;
  int total_ttime=0;

  for (int i=0; i<NPROCS; i++) {
    int r, w;
    int pid = waitx(0, &r, &w);
    int turnaround = r+w;

    total_rtime += r;
    total_wtime += w;
    total_ttime += turnaround;

    printf("Process PID %d: RunTime = %d ticks, WaitTime = %d ticks, TurnaroundTime = %d ticks\n",
           pid, r, w, turnaround);
  }

  printf("\nSummary Statistics\n");
  printf("Average Response / Running Time: %d ticks\n", total_rtime / NPROCS);
  printf("Average Waiting Time: %d ticks\n", total_wtime / NPROCS);
  printf("Average Turnaround Time: %d ticks\n", total_ttime / NPROCS);

  exit(0);
}
