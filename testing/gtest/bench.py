#!/usr/bin/env python3

import sys, subprocess, json, statistics

proc = subprocess.Popen(["./mach", "gtest", sys.argv[1]], stdout=subprocess.PIPE)
for line in proc.stdout:
    if line.startswith(b"PERFHERDER_DATA:"):
        data = json.loads(line[len("PERFHERDER_DATA:"):].decode("utf8"))
        for suite in data["suites"]:
            for subtest in suite["subtests"]:
                print("%4d.%03d ± %6s ms    %s.%s" % (
                    subtest["value"] / 1000.,
                    subtest["value"] % 1000,
                    "%.3f" % (statistics.stdev(subtest["replicates"]) / 1000),
                    suite["name"],
                    subtest["name"],
                ))
