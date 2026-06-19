# pc_host/services/chart_service.py
import csv
from datetime import datetime
import matplotlib.pyplot as plt


def append_history_row(path: str, event_name: str, value: str) -> None:
    with open(path, "a", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([datetime.now().isoformat(timespec="seconds"), event_name, value])


def build_history_figure(csv_path: str):
    counts = {}
    with open(csv_path, "r", encoding="utf-8") as handle:
        for _stamp, event_name, _value in csv.reader(handle):
            counts[event_name] = counts.get(event_name, 0) + 1
    figure, axis = plt.subplots(figsize=(6, 3))
    axis.bar(list(counts.keys()), list(counts.values()))
    axis.set_title("PC Host Event Summary")
    axis.set_ylabel("Count")
    return figure
