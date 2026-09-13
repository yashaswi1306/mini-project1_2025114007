# Report 

## 2.3.1 Implementation Summary  

1. Makefile  

```
ifdef SCHEDULER # eg. scheduler=fifo
CFLAGS += -D$(SCHEDULER) -DSCHEDULER_$(SCHEDULER) #flags for compilation
endif
```

when no SCHEDULER passed, returns to Round Robin scheduling.  

2. struct proc in proc.h  

added priority metric (0-3), ticks used , ctime(creation), rtime (running time),etime (exit time), wtime (waiting time) and first_run (initial execution tick)  

3. allocproc() changes  

initialised:
``` 
priority to 0 (highest pq)
ticks used to 0 
ctime to curr suystem ticks (created at curr ticks)
rtime, etime and wtime to 0
first_run to -1 since proc hasnt yet been scheduled
```


4. queue selection/preemption logic  

RR: preemptive rr via runnable processes  
MLFQ: preemptive multi queue scheduling (scans queue in order of 0 to 3). if process in higher queue becomes runnable, scheduler pre empts curr process.  
FIFO: non preemptice. scheduler searched process table fr earliest ctime for runnable proc. so timer interrupt preemptive yield() is disabled under FIFO.  

5. time slice handling:  
kernel/trap.c increments ticks used for a running process.

Given in assignment: 
```
Q0 : 1 tick
Q1 : 4 tick
Q2 : 8 tick
Q3 : 16 tick
```

if ticks used reached queue limit (1,4,8,16), priority increased by 1 (demoted)  

6. voluntary yield handling  

when in proc.c proces yields control (sleep() or for io), ticks counter is reset to 0 and priority level is left the same. THus, io bound tasks (interactive) retain high priority.  

7. priority boosting:  

to eliminate cpu starvation, cpu bound processes occupying lower queues are restored to pq 0 (high prority), when ticks%48==0 (every 48 ticks), and their ticks used is restored to 0.  

8. procdump changes:

```
#ifdef SCHEDULER_MLFQ
    printk("%d %s %s priority=%d ticks_used=%d\n", p->pid, p->name, state, p->priority, p->ticks_used);
#else
    printk("%d %s %s\n", p->pid, state, p->name);
#endif
```

Trigerred via ctrl p. THis is because debugging support said to print 
```
PID, name, state, current priority/queue number, and any other bookkeeping data relevant to verifying scheduler correctness (e.g. ticks consumed in current slice, ticks since last boost)
```

for each process.  

## 2.3.2 MLFQ Analysis  

schedtest tests how scheduler peforms under  

1. Heavy computational load  
2. IO bound processes  

pid 5 (orange) : this is the io bound interactie process. It runs only for a sgort period before stopping to yield to wait for input. Since it gives cpu back so frequently and beofre a time slice completes, its priority remains high (0 and 1), as the tick counter us constantly reset to 0.  

pid 4 (blue) : this finishes its allocated ticks and gets down on the q. But the shift from q2 to q3 is at 20 not 13 to simulate a yield for io. So, it sits in q2 for longer, before getting demoted to q3. THis is medium cpu bound.  

pid 3 (pink) : this is a heavily cpu bound process, and drops to last priority at 13 secs (1+4+8), as it never yields control for sleep or io.  

boost: the global prioirty boost occurs at every 48 seconds. proc 5 isnt much affected by it, because the use of a priority boost is to make sure cpu bound tasks are not starved for execution time. So, at 48 and 96, all processes are returned to priority 0, which helps prevent starvaion of pid 4 and pid 3.

## 2.3.3 Comparison Results

| Scheduler policy | Avg Turnaround TIme (ticks) | Avg waiting time (ticks) | Avg response time (ticks)|
| --- | --- | --- | --- | 
| FIFO | 8 | 4 | 0 | 
| MLFQ | 4 | 0 | 0 |
| RR | 4 | 0 | 0 |

FIFO has the highest turnaround time and the highest wait time.  
THis is beacause FIFO is NON preemptive, meaning that quick tasks get stuck behind longer ones.  
RR has its wait time depend upon quantam size.  
So if quantam is small, theres a ton of context switching. Else, it behaves as a FFO system.  
MLFQ has least waiting time as it keeps io bound tasks at high priority and cpu bound ones at lower probability.