#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  //added
  p->priority = 0; //new proc starts in pq 0
  p->ticks_used = 0; //no ticks used till now

  acquire(&tickslock); //lock proc
  p->ctime = ticks; //store global tiks counter as process creation time
  release(&tickslock); //release proc
  p->rtime = 0;
  p->wtime = 0;
  p->etime = 0;
  p->first_run = -1;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline,
               PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE, (uint64)(p->trapframe),
               PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0) {
    if (sz + n > TRAPFRAME) {
      return -1;
    }
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if (n < 0) {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++) {
    if (pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().

void
kexit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);


//added 

//get exact timer ticks when process exited so u can calculate the turnaround and stuff
  acquire(&p->lock);

  acquire(&tickslock);
  p->etime = ticks;
  release(&tickslock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent == p) {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE) {
          // Found one.
          pid = pp->pid;
          if (addr != 0 &&
              copyout(p->pagetable, p->sz, addr, (char *)&pp->xstate,
                      sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          pp->parent = 0;
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep_prepare(p); //DOC: wait-sleep
    release(&wait_lock);
    sleep();
    acquire(&wait_lock);
  }
}

// Extended wait system call: waitx
//added 
int
waitx(uint64 addr, uint64 rtime, uint64 wtime, uint64 retime) 
{
  struct proc *pp; // ptr to loop thru procs
  int havekids, pid; // flags for kids and thier id
  struct proc *p = myproc(); // get curnt proc

  acquire(&wait_lock); // grab global wait lock

  for (;;) { 
    havekids = 0; // reset kids flag
    for (pp = proc; pp < &proc[NPROC]; pp++) { // check evry proc
      if (pp->parent == p) { // is proc parent of kid?
        acquire(&pp->lock); // lock the kid
        havekids = 1; // have at least one kid
        if (pp->state == ZOMBIE) { 
          pid = pp->pid; // if kids a zombie, save id before del
          if (addr != 0 && copyout(p->pagetable, p->sz, addr, (char *)&pp->xstate, sizeof(pp->xstate)) < 0) { // copy exit stat
            release(&pp->lock); // drop kid lock on fail
            release(&wait_lock); // drop global lock
            return -1; // return error
          } // end if
          if (rtime != 0 && copyout(p->pagetable, p->sz, rtime, (char *)&pp->rtime, sizeof(pp->rtime)) < 0) { // copy rtime if asked
            release(&pp->lock); // drop lock on failure
            release(&wait_lock); // drop wait lock
            return -1; // error out
          } // end if
          if (wtime != 0 && copyout(p->pagetable, p->sz, wtime, (char *)&pp->wtime, sizeof(pp->wtime)) < 0) { // copy wtime if asked
            release(&pp->lock); // failure so drop lock
            release(&wait_lock); // drop wait lock
            return -1; // error out
          } 
          if (retime != 0) {
            int response_time = (pp->first_run >= 0) ? (pp->first_run - pp->ctime) : 0;
            if (copyout(p->pagetable, p->sz, retime, (char *)&response_time, sizeof(response_time)) < 0) {
              release(&pp->lock);
              release(&wait_lock);
              return -1;
            }
          }

          pp->parent = 0; // rmove parent ties
          freeproc(pp); // free  proc
          release(&pp->lock); // unlock kid
          release(&wait_lock); // unlock global lock
          return pid; // return dead kids id
        } 
        release(&pp->lock); // not dead so unlock
      } 
    } 

    if (!havekids || killed(p)) { // proc has no kids nd isnt killed yet
      release(&wait_lock); // rem lock
      return -1; // return error
    } 

    sleep_prepare(p); // prep for sleep
    release(&wait_lock); // drop lock nd sleep
    sleep(); 
    acquire(&wait_lock);
  } 
} 
  
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
#ifdef SCHEDULER_FIFO //added
// FIFO scheduler implementation (Non-preemptive, run earliest created process to completion)
void
scheduler(void) // main sched functin (fifo)
{ 
  struct proc *p; // ptr for looping procs
  struct cpu *c = mycpu(); // get crnt cpu

  c->proc = 0; 
  for (;;) { 
    intr_on(); // turn on interupts
    intr_off(); // disable cuz scheduler needs to first (opens so sys can acknowledge a timer tick or I/O event.)

    struct proc *earliest = 0; // track oldst proc
    for (p = proc; p < &proc[NPROC]; p++) { 
      acquire(&p->lock); // lock the proc
      if (p->state == RUNNABLE) { 
        if (earliest == 0 || p->ctime < earliest->ctime) { // check if oldr
          if (earliest) release(&earliest->lock); // unlock prev oldst
          earliest = p; // save new oldst
          continue; // go to nxt proc
        } 
      } 
      release(&p->lock); // unlock proc
    } 

    if (earliest) { 
      if (earliest->first_run == -1) { //first time running
        acquire(&tickslock); // grab time lock
        earliest->first_run = ticks; // recrod first run time
        release(&tickslock); 
      } 
      earliest->state = RUNNING; // mark as runing
      c->proc = earliest; // set cpu proc ptr
      swtch(&c->context, &earliest->context); // ctx switch to it

      mycpu()->intena = 0; // disble interupts
      c->proc = 0; // clear cpu proc again
      release(&earliest->lock);
    } else { // if no proc found
      asm volatile("wfi"); // wait for interupt
    }
  } 
} 

#elif defined(SCHEDULER_MLFQ) // check if mlfq is scheduler
// MLFQ scheduler implementation 
static int mlfq_ticks_since_boost = 0; // track ticks for boost

void
scheduler(void) // the sched functin
{ 
  struct proc *p; // ptr for procs
  struct cpu *c = mycpu(); // get our cpu

  c->proc = 0; // no proc yet
  for (;;) {
    intr_on(); 
    intr_off(); 

    // Acquire tick lock to safely handle global boosting
    acquire(&tickslock); 
    // Global priority boost every 48 ticks (given in mp doc)
    if (ticks - mlfq_ticks_since_boost >= 48) { // time to boost
      mlfq_ticks_since_boost = ticks; // resest boost timer
      for (struct proc *bp = proc; bp < &proc[NPROC]; bp++) { 
        acquire(&bp->lock); // lock proc for boost
        bp->priority = 0; // bump to top priority
        bp->ticks_used = 0; // rst ticks usd
        release(&bp->lock); // let proc go
      } 
    } 

    release(&tickslock); // drop tick lock

    int found = 0; // flag if we got one
    // Iterate through priority queues 0 (highest) to 3 (lowest)
    for (int q = 0; q <= 3; q++) { 
      for (p = proc; p < &proc[NPROC]; p++) { // loop evry proc
        acquire(&p->lock); // lock it
        if (p->state == RUNNABLE && p->priority == q) { // is it ready and in this q?
          p->state = RUNNING; // set runing
          c->proc = p; // give to cpu
          swtch(&c->context, &p->context); // jump into it

          mycpu()->intena = 0; // turn off interrupts 
          c->proc = 0; // clr cpu proc
          found = 1; // mark as found
        } 
        release(&p->lock); // unlock it
        if (found) break; // exit loop if found
      } 
      if (found) break; // exit q loop if found
    } 

    if (!found) { 
      asm volatile("wfi"); // wait for interrupts
    } 
  } 
} 

#else
// Standard Round-Robin Scheduler
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;) {
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    int found = 0;
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Don't re-enable interrupts on release.
        mycpu()->intena = 0;

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
}
#endif

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched RUNNING");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (__atomic_load_n(&first, __ATOMIC_ACQUIRE)) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    // ensure other cores see first=0.
    __atomic_store_n(&first, 0, __ATOMIC_RELEASE);

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){"/init", 0});
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Register current process as waiting for wakeups on chan.
void
sleep_prepare(void *chan)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  if (chan == 0)
    panic("sleep_prepare: zero chan");
  p->chan = chan;
  release(&p->lock);
}

// Put the thread to sleep.  Assumes sleep_prepare() was called before.
// If the channel registered by sleep_prepare() has been woken up in
// the meantime, do not go to sleep, and instead return immediately.
//added
void
sleep(void)
{
  struct proc *p = myproc(); //get curr roc

  acquire(&p->lock); //lock before changing
  if (p->chan != 0) { //if it has sleep channel
    p->state = SLEEPING; //aek as sleeping
#ifdef SCHEDULER_MLFQ
    p->ticks_used = 0; //reset ti ks when proc sleeps
#endif
    sched(); //switch to scheduler
  }
  release(&p->lock);
}

// Wake up all processes sleeping on channel chan.
void
wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->chan == chan) {
      // If the process is waiting for wakeups on this channel,
      // signal that the wakeup happened by clearing p->chan.
      p->chan = 0;

      // If this waiting process has gotten so far as to actually
      // go to sleep, also set it back to RUNNING.
      if (p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
    }
    release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING) {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst) {
    return copyout(p->pagetable, p->sz, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src) {
    return copyin(p->pagetable, p->sz, dst, src, len);
  } else {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
    // clang-format off
    [UNUSED]    = "unused",
    [USED]      = "used",
    [SLEEPING]  = "sleep ",
    [RUNNABLE]  = "runble",
    [RUNNING]   = "run   ",
    [ZOMBIE]    = "zombie"
    // clang-format on
  };
  struct proc *p;
  char *state;

  printk("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
//added
#ifdef SCHEDULER_MLFQ
    printk("%d %s %s priority=%d ticks_used=%d\n", p->pid, p->name, state,p->priority, p->ticks_used);
#else
    printk("%d %s %s\n", p->pid, state, p->name);
#endif
  }
}