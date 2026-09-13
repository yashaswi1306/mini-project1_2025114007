import matplotlib.pyplot as plt 
import numpy as np 

plt.figure(figsize=(12, 6))
# ticks timeline (0-120 ticks)
ticks = np.arange(0, 120, 1) # time array from 0 to 119

# simulate 3 processes moving through MLFQ queues
# pid3: heavy cpu process
pid3_q = [] # qstate for proc 3
for t in ticks: # for each tick
    cycle_t = t % 48 # find pos in 48 tick cycle

    if cycle_t < 1: # just startd or just boosted
        pid3_q.append(0) # q0

    elif cycle_t < 5: # used up first q time
        pid3_q.append(1) # drop to q1
    elif cycle_t < 13: # usd up scnd q time
        pid3_q.append(2) # drop to q2
    else: 
        pid3_q.append(3) 

# pid4: medium CPU process
pid4_q = [] # states list proc4
for t in ticks: 
    cycle_t = t % 48 
    if cycle_t < 1: 
        pid4_q.append(0) # q0
    elif cycle_t < 5: # scnd level
        pid4_q.append(1) # q1
    elif cycle_t < 20: # fakes small io
        pid4_q.append(2) # q2
    else: 
        pid4_q.append(3) # q3

# pid5: I/O-bound process (stays in high priority queues 0 and 1)
pid5_q = [] 
for t in ticks:
    cycle_t = t % 48 # get cycle pos
    if cycle_t % 4 == 0: # fakes waking up from io
        pid5_q.append(0) # goes bck to top q
    else: 
        pid5_q.append(1) # drops one level then sleeps

plt.step(ticks, pid3_q, label='PID3 (CPU-Bound Heavy)', where='post', linewidth=3, color="#e73ca5") # pink = p3 where = post for star step lines
plt.step(ticks, pid4_q, label='PID4 (CPU-Bound Medium)', where='post', linewidth=3, color="#2274e6") # blue = p4
plt.step(ticks, pid5_q, label='PID5 (I/O-Bound Interactive)', where='post', linewidth=3, color="#e17019") # orange = p5

# Mark Priority Boost lines at 48 and 96 ticks 
plt.axvline(x=48, color='black', linestyle='--', alpha=0.5, label='Priority Boost (48 Ticks)') # frst boost line
plt.axvline(x=96, color='black', linestyle='--', alpha=0.5) # scnd boost line

plt.xlabel('Time Elapsed(ticks)', fontsize=10) 
plt.ylabel('Queue ID (0 = Highest Priority, 3 = Lowest)', fontsize=10) 
plt.title('MLFQ Scheduler Process Queue Transitions Over Time', fontweight='bold',fontsize=12)
plt.gca().invert_yaxis() # 0 at top, as in flip y axis so 0 is high
plt.yticks([0, 1, 2, 3], ['Queue 0: 1 tick', 'Queue 1: 4 ticks', 'Queue 2: 8 ticks', 'Queue 3: 16 ticks']) # name y ticks
plt.grid(True, linestyle=':', alpha=0.5) # turn on dot matrix gridd
plt.legend(loc='right') # legend

# watermark
plt.text(60, 2, 'yashaswi.priya', fontsize=20, color='black', alpha=0.2)

plt.tight_layout() # fix spacng 
plt.savefig('mlfq_timeline.png', dpi=100)