import matplotlib.pyplot as plt
import numpy as np
import json

cases = ("test_exception",)
runtimes = ("eff-unwind", "Koka", "cpp-effects", "C++")
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
            if result["command"].startswith("./vanilla-cpp/bin/"):
                penguin_means["C++"][caseidx] = result["mean"] * 1000
                penguin_stddev["C++"][caseidx] = result["stddev"] * 1000


x = np.arange(len(cases))  # the label locations
width = 0.1  # the width of the bars
multiplier = 0

fig, ax = plt.subplots(layout="constrained")

for attribute, measurement in penguin_means.items():
    offset = width * multiplier
    rects = ax.barh(
        x + offset, measurement, width, label=attribute, xerr=penguin_stddev[attribute]
    )
    ax.bar_label(rects, padding=3)
    multiplier += 1

# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_xlabel("Time (ms)")
# ax.set_title("Execution time by implementation")
ax.set_yticks(x + width, cases, rotation=45)
ax.legend(loc="upper center", bbox_to_anchor=(0.4, -0.3), ncol=4)
ax.set_xlim(0, 4_000)
ax.invert_yaxis()

fig.set_figwidth(5.08)
fig.set_figheight(1.8)

# plt.show()
plt.savefig("figure8.pdf")
