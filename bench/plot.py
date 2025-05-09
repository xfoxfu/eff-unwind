import matplotlib.pyplot as plt
import numpy as np
import json

cases = (
    "fibonacci_recursive",
    "countdown",
    # "generator",
    "handler_sieve",
    "iterator",
    "parsing_dollars",
    "resume_nontail",
    # "test_exception",
    "triples",
    "product_early",
)
runtimes = ("eff-unwind", "Koka", "cpp-effects", "OCaml")
penguin_means = {}
penguin_stddev = {}
for runtime in runtimes:
    penguin_means[runtime] = [0 for _ in cases]
    penguin_stddev[runtime] = [0 for _ in cases]

for case in cases:
    print(case)
    with open(f"./{case}.json") as f:
        results = json.load(f)["results"]
        for result in results:
            caseidx = cases.index(case)
            if result["command"].startswith("../build/"):
                penguin_means["eff-unwind"][caseidx] = result["mean"] * 1000
                penguin_stddev["eff-unwind"][caseidx] = result["stddev"] * 1000
            if result["command"].startswith("./koka/bin/"):
                penguin_means["Koka"][caseidx] = result["mean"] * 1000
                penguin_stddev["Koka"][caseidx] = result["stddev"] * 1000
            if result["command"].startswith("./cpp-effects/bin/"):
                penguin_means["cpp-effects"][caseidx] = result["mean"] * 1000
                penguin_stddev["cpp-effects"][caseidx] = result["stddev"] * 1000
            if result["command"].startswith("./ocaml/bin/"):
                penguin_means["OCaml"][caseidx] = result["mean"] * 1000
                penguin_stddev["OCaml"][caseidx] = result["stddev"] * 1000


x = np.arange(len(cases))  # the label locations
width = 0.2  # the width of the bars
multiplier = 0

fig, ax = plt.subplots(layout="constrained")

for attribute, measurement in penguin_means.items():
    offset = width * multiplier
    rects = ax.barh(
        x + offset, measurement, width, label=attribute, xerr=penguin_stddev[attribute]
    )
    labels = ax.bar_label(
        rects,
        labels=[f"{m:.2f}" if m > 0 else "N/A" for m in measurement],
        padding=3,
        fontsize=9,
    )
    multiplier += 1

# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_xlabel("Time (ms)")
ax.set_title("Execution time by implementation")
ax.set_yticks(x + width, cases, rotation=45)
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=5)
ax.set_xlim(0, 5_000)
ax.invert_yaxis()

ax0 = ax.twinx()
ax0.set_ylim(ax.get_ylim())
ticks = [
    i * width + j
    for i, (attribute, measurement) in enumerate(penguin_means.items())
    for j, m in enumerate(measurement)
    if m > 5_000
]
labels = [
    f"{m:.2f}"
    for i, (attribute, measurement) in enumerate(penguin_means.items())
    for j, m in enumerate(measurement)
    if m > 5_000
]
ax0.set_yticks(ticks, labels, fontsize=9)

fig.set_figwidth(11.69)
fig.set_figheight(5.845)

# plt.show()
plt.savefig("figure7.pdf")
