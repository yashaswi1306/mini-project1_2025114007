import re
import pandas as pd

def generate_csv_from_pasted_data():
    # Paste your raw terminal outputs right here between the triple quotes:
    raw_output = """
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ schedtest
Scheduler Benchmark Test (NPROCS=4)
MLFQ_TRACE: pid=4,elapsed=28,queue=1,event=DEMOTE
MLFQ_TRACE: pid=6,elapsed=29,queue=1,event=DEMOTE
MLFQ_TRACE: pid=4,elapsed=32,queue=2,event=DEMOTE
MLFQ_TRACE: pid=6,elapsed=33,queue=2,event=DEMOTE
Process PID 4: RunTime = 7, WaitTime = 0, TurnaroundTime = 7, ResponseTime = 0
Process PID 6: RunTime = 7, WaitTime = 0, TurnaroundTime = 7, ResponseTime = 0
MLFQ_TRACE: pid=5,elapsed=48,queue=0,event=BOOST
MLFQ_TRACE: pid=7,elapsed=48,queue=0,event=BOOST
Process PID 5: RunTime = 0, WaitTime = 9, TurnaroundTime = 9, ResponseTime = 0
Process PID 7: RunTime = 0, WaitTime = 14, TurnaroundTime = 14, ResponseTime = 0

Summary Statistics
Average Running Time: 3 ticks
Average Waiting Time: 5 ticks
Average Turnaround Time: 9 ticks
Average Response Time: 0 ticks
$ schedtest
MLFQ_TRACE: pid=8,elapsed=85,queue=1,event=DEMOTE
Scheduler Benchmark Test (NPROCS=4)
MLFQ_TRACE: pid=9,elapsed=87,queue=1,event=DEMOTE
MLFQ_TRACE: pid=12,elapsed=88,queue=1,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=88,queue=1,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=92,queue=2,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=92,queue=2,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=96,queue=0,event=BOOST
MLFQ_TRACE: pid=10,elapsed=96,queue=0,event=BOOST
MLFQ_TRACE: pid=11,elapsed=96,queue=0,event=BOOST
MLFQ_TRACE: pid=12,elapsed=96,queue=0,event=BOOST
MLFQ_TRACE: pid=11,elapsed=96,queue=1,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=97,queue=1,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=100,queue=2,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=101,queue=2,event=DEMOTE
Process PID 10: RunTime = 0, WaitTime = 19, TurMLFQ_TRACE: pid=11,elapsed=108,qnueue=3,event=DEMOTE
aMLFQ_TRACE: pid=9,elapsed=108,queue=3,event=DEMOTE
roundTime = 19, ResponseTime = 0
Process PID 12: RunTime = 1, WaitTime = 24, TurnaroundTime = 25, ResponseTime = 0
MLFQ_TRACE: pid=9,elapsed=144,queue=0,event=BOOST
MLFQ_TRACE: pid=11,elapsed=144,queue=0,event=BOOST
MLFQ_TRACE: pid=9,elapsed=144,queue=1,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=144,queue=1,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=148,queue=2,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=148,queue=2,event=DEMOTE
MLFQ_TRACE: pid=9,elapsed=156,queue=3,event=DEMOTE
MLFQ_TRACE: pid=11,elapsed=156,queue=3,event=DEMOTE
Process PID 9: RunTime = 79, WaitTime = 1, TurnaroundTime = 80, ResponseTime = 0
Process PID 11: RunTime = 82, WaitTime = 1, TurnaroundTime = 83, ResponseTime = 0

Summary Statistics
Average Running Time: 40 ticks
Average Waiting Time: 11 ticks
Average Turnaround Time: 51 ticks
Average Response Time: 0 ticks
$ schedtest
Scheduler Benchmark Test (NPROCS=4)
MLFQ_TRACE: pid=14,elapsed=219,queue=1,event=DEMOTE
MLFQ_TRACE: pid=16,elapsed=219,queue=1,event=DEMOTE
MLFQ_TRACE: pid=14,elapsed=223,queue=2,event=DEMOTE
MLFQ_TRACE: pid=16,elapsed=223,queue=2,event=DEMOTE
Process PID 14: RunTime = 7, WaitTime = 0, TurnaroundTime = 7, ResponseTime = 0
Process PID 16: RunTime = 6, WaitTime = 0, TurnaroundTime = 6, ResponseTime = 0
Process PID 15: RunTime = 0, WaitTime = 6, TurnaroundTime = 6, ResponseTime = 0
Process PID 17: RunTime = 0, WaitTime = 6, TurnaroundTime = 6, ResponseTime = 0

Summary Statistics
Average Running Time: 3 ticks
Average Waiting Time: 3 ticks
Average Turnaround Time: 6 ticks
Average Response Time: 0 ticks
$ QEMU: Terminated
    """

    events = []
    
    # Parse every line matching MLFQ_TRACE
    for line in raw_output.splitlines():
        # Handle cases where trace logs might be slightly spliced or concatenated
        matches = re.finditer(r"MLFQ_TRACE:\s*pid=(\d+),\s*elapsed=(\d+),\s*queue=(\d+),\s*event=(\w+)", line)
        for m in matches:
            events.append({
                "pid": int(m.group(1)),
                "elapsed": int(m.group(2)),
                "queue": int(m.group(3)),
                "event": m.group(4)
            })

    if len(events) > 0:
        df = pd.DataFrame(events)
        df.to_csv("mlfq_timeline_data.csv", index=False)
        print(f"SUCCESS! Extracted {len(events)} real trace events and generated 'mlfq_timeline_data.csv'.")
    else:
        print("Error: No trace events found. Check the pasted text format.")

if __name__ == "__main__":
    generate_csv_from_pasted_data()