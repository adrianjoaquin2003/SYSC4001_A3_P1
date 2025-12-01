import os
import subprocess
import difflib
import json
import re
import matplotlib.pyplot as plt

# ----------------------------------------------
# CONFIGURATION
# ----------------------------------------------

TEST_DIR = "input_files/test_cases"
OUTPUT_DIR = "output_files/results"
CHART_DIR = "results/charts"
EXPECTED_DIR = "expected_outputs"   # still optional

EXECUTABLES = {
    "EP": "./bin/interrupts_EP",
    "RR": "./bin/interrupts_RR",
    "EP_RR": "./bin/interrupts_EP_RR"
}

os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(CHART_DIR, exist_ok=True)


# ----------------------------------------------
# BASIC FILE HELPERS
# ----------------------------------------------

def read_file(path):
    if not os.path.exists(path):
        return None
    with open(path, "r") as f:
        return f.read()


# ----------------------------------------------
# PARSING TRANSITIONS FROM execution.txt
# ----------------------------------------------

def parse_transitions(exec_text):
    transitions = []
    for line in exec_text.splitlines():
        m = re.match(r"^\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\w+)\s*\|\s*(\w+)\s*\|", line)
        if m:
            transitions.append({
                "time": int(m.group(1)),
                "pid": int(m.group(2)),
                "old": m.group(3),
                "new": m.group(4)
            })
    return transitions


# ----------------------------------------------
# PARSE ARRIVAL TIMES FROM INPUT FILE
# ----------------------------------------------

def parse_input_file(path):
    arrivals = {}
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if line == "":
                continue
            parts = [p.strip() for p in line.split(",")]
            pid = int(parts[0])
            arrival = int(parts[2])
            arrivals[pid] = arrival
    return arrivals


# ----------------------------------------------
# METRICS COMPUTATION
# ----------------------------------------------

def compute_metrics(transitions, arrivals):
    first_run = {}
    terminate_time = {}
    ready_start = {}
    total_ready_wait = {}

    # Init
    for pid in arrivals:
        total_ready_wait[pid] = 0
        ready_start[pid] = None

    for t in transitions:
        pid = t["pid"]
        old = t["old"]
        new = t["new"]
        time = t["time"]

        # Response time
        if new == "RUNNING" and pid not in first_run:
            first_run[pid] = time

        # READY interval tracking
        if new == "READY":
            ready_start[pid] = time

        if new == "RUNNING":
            if ready_start[pid] is not None:
                total_ready_wait[pid] += (time - ready_start[pid])
                ready_start[pid] = None

        if new == "TERMINATED":
            terminate_time[pid] = time

    num_procs = len(arrivals)

    total_time = max(terminate_time.values()) if terminate_time else 0
    throughput = float(num_procs) / float(total_time) if total_time else 0

    wait_times = []
    resp_times = []
    turn_times = []

    for pid in arrivals:
        arrival = arrivals[pid]

        resp = first_run.get(pid, 0) - arrival
        resp_times.append(resp)

        turn = terminate_time.get(pid, 0) - arrival
        turn_times.append(turn)

        wait_times.append(total_ready_wait.get(pid, 0))

    return {
        "throughput": throughput,
        "avg_wait_time": sum(wait_times) / num_procs,
        "avg_turnaround_time": sum(turn_times) / num_procs,
        "avg_response_time": sum(resp_times) / num_procs
    }


# ----------------------------------------------
# GANTT CHART GENERATION
# ----------------------------------------------

def generate_gantt_chart(transitions, sched_name, test_file):
    """
    Builds a Gantt chart by detecting RUNNING durations per PID.
    """
    segments = []
    running_start = {}

    for t in transitions:
        pid = t["pid"]
        time = t["time"]
        new = t["new"]

        if new == "RUNNING":
            running_start[pid] = time

        if new in ["READY", "WAITING", "TERMINATED"]:
            if pid in running_start:
                segments.append((pid, running_start[pid], time))
                del running_start[pid]

    if len(segments) == 0:
        return

    plt.figure(figsize=(10, 4))
    for pid, start, end in segments:
        plt.barh(pid, end - start, left=start)

    plt.xlabel("Time")
    plt.ylabel("PID")
    plt.title(f"Gantt Chart — {sched_name} — {test_file}")
    plt.tight_layout()

    save_path = f"{CHART_DIR}/Gantt_{sched_name}_{test_file}.png"
    plt.savefig(save_path)
    plt.close()


# ----------------------------------------------
# RUN TEST CASE
# ----------------------------------------------

def run_test_case(exec_name, exec_path, test_file):
    input_path = f"{TEST_DIR}/{test_file}"

    print(f"\n--- Running {exec_name} on {test_file} ---")

    subprocess.run([exec_path, input_path], check=True)

    exec_text = read_file("execution.txt")
    transitions = parse_transitions(exec_text)
    arrivals = parse_input_file(input_path)

    metrics = compute_metrics(transitions, arrivals)

    with open(f"{OUTPUT_DIR}/{exec_name}_{test_file}_metrics.json", "w") as f:
        json.dump(metrics, f, indent=4)

    with open(f"{OUTPUT_DIR}/{exec_name}_{test_file}.out", "w") as f:
        f.write(exec_text)

    # Gantt chart
    generate_gantt_chart(transitions, exec_name, test_file)

    return metrics


# ----------------------------------------------
# METRIC COMPARISON CHARTS
# ----------------------------------------------

def generate_comparison_charts(test_files):
    for test_file in test_files:
        metrics = {}

        # Load metrics for each scheduler
        for sched in EXECUTABLES.keys():
            json_path = f"{OUTPUT_DIR}/{sched}_{test_file}_metrics.json"
            if os.path.exists(json_path):
                with open(json_path, "r") as f:
                    metrics[sched] = json.load(f)

        if not metrics:
            continue

        # For each metric, produce comparison chart
        for metric in ["throughput", "avg_wait_time", "avg_turnaround_time", "avg_response_time"]:
            plt.figure()

            schedulers = list(metrics.keys())
            values = [metrics[s][metric] for s in schedulers]

            plt.bar(schedulers, values)
            plt.title(f"{metric} comparison — {test_file}")
            plt.xlabel("Scheduler")
            plt.ylabel(metric)
            plt.tight_layout()

            plt.savefig(f"{CHART_DIR}/Compare_{metric}_{test_file}.png")
            plt.close()


# ----------------------------------------------
# MAIN
# ----------------------------------------------

def main():
    test_files = sorted(os.listdir(TEST_DIR))

    metrics_summary = {}

    for sched_name, exec_path in EXECUTABLES.items():
        for test_file in test_files:
            m = run_test_case(sched_name, exec_path, test_file)
            metrics_summary[(sched_name, test_file)] = m

    # Comparison charts
    generate_comparison_charts(test_files)

    print("\nCharts generated in:", CHART_DIR)


if __name__ == "__main__":
    main()
