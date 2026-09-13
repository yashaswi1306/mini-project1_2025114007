import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("mlfq_timeline_data.csv")

# Ensure START events exist for processes so lines start correctly at Q0
base_events = []
for pid in df["pid"].unique():
    subset = df[df["pid"] == pid]
    min_elapsed = subset["elapsed"].min()
    # If it doesn't have an event at elapsed=0 or near its start, prepend a START at queue 0
    if not ((subset["elapsed"] == 0) & (subset["queue"] == 0)).any():
        # Find a reasonable start time (e.g., slightly before its first demote/boost)
        start_time = max(0, min_elapsed - 2)
        base_events.append({"pid": pid, "elapsed": start_time, "queue": 0, "event": "START"})

if base_events:
    df = pd.concat([pd.DataFrame(base_events), df], ignore_index=True)

fig, ax = plt.subplots(figsize=(11, 6))

for pid in sorted(df["pid"].unique()):
    part = df[df["pid"] == pid].sort_values("elapsed")
    ax.step(part["elapsed"], part["queue"], where="post", linewidth=2, label=f"PID {pid}")
    ax.scatter(part["elapsed"], part["queue"], s=30)
    for _, row in part.iterrows():
        if row["event"] in ["DEMOTE", "BOOST"]:
            q_label = f"Q{int(row['queue'])}" if row["event"] == "DEMOTE" else "Boost"
            ax.annotate(q_label, (row["elapsed"], row["queue"]), xytext=(3, 5), textcoords="offset points", fontsize=7)

# Priority boost lines
for elapsed in [48, 96, 144]:
    ax.axvline(elapsed, color="gray", linestyle="--", linewidth=1.2, alpha=0.7)
    ax.text(elapsed - 2, 3.3, f"Boost @ {elapsed}", rotation=90, va="top", fontsize=8, color="dimgray")

ax.set_xlabel("Time elapsed since scheduler test start (ticks)", fontsize=10)
ax.set_ylabel("Queue ID (0 = highest priority)", fontsize=10)
ax.set_yticks([0, 1, 2, 3])
ax.set_ylim(3.5, -0.5)
ax.set_xlim(-2, df["elapsed"].max() + 10)
ax.set_title("MLFQ Queue Transitions from schedulertest", fontsize=12, pad=10)
ax.grid(True, alpha=0.2)
ax.legend(title="Process", bbox_to_anchor=(1.02, 1), loc="upper left", fontsize=8)
ax.text(0.99, 0.02, "yashaswi.priya", transform=ax.transAxes, ha="right", va="bottom", fontsize=8, alpha=0.7)

fig.tight_layout()
plt.savefig("mlfq_timeline.png", dpi=200, bbox_inches="tight")
print("Successfully generated clean mlfq_timeline.png!")