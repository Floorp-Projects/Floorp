# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform passes options from `mach perftest` to the corresponding task.
"""


import json
from copy import deepcopy
from datetime import date, timedelta

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from taskgraph.util.treeherder import join_symbol, split_symbol
from voluptuous import Any, Extra, Optional

transforms = TransformSequence()


perftest_description_schema = Schema(
    {
        # The test names and the symbols to use for them: [test-symbol, test-path]
        Optional("perftest"): [[str]],
        # Metrics to gather for the test. These will be merged
        # with options specified through perftest-perfherder-global
        Optional("perftest-metrics"): optionally_keyed_by(
            "perftest",
            Any(
                [str],
                {str: Any(None, {str: Any(None, str, [str])})},
            ),
        ),
        # Perfherder data options that will be applied to
        # all metrics gathered.
        Optional("perftest-perfherder-global"): optionally_keyed_by(
            "perftest", {str: Any(None, str, [str])}
        ),
        # Extra options to add to the test's command
        Optional("perftest-extra-options"): optionally_keyed_by("perftest", [str]),
        # Variants of the test to make based on extra browsertime
        # arguments. Expecting:
        #    [variant-suffix, options-to-use]
        # If variant-suffix is `null` then the options will be added
        # to the existing task. Otherwise, a new variant is created
        # with the given suffix and with its options replaced.
        Optional("perftest-btime-variants"): optionally_keyed_by(
            "perftest", [[Any(None, str)]]
        ),
        # These options will be parsed in the next schemas
        Extra: object,
    }
)


transforms.add_validate(perftest_description_schema)


@transforms.add
def split_tests(config, jobs):
    for job in jobs:
        if job.get("perftest") is None:
            yield job
            continue

        for test_symbol, test_name in job.pop("perftest"):
            job_new = deepcopy(job)

            job_new["perftest"] = test_symbol
            job_new["name"] += "-" + test_symbol
            job_new["treeherder"]["symbol"] = job["treeherder"]["symbol"].format(
                symbol=test_symbol
            )
            job_new["run"]["command"] = job["run"]["command"].replace(
                "{perftest_testname}", test_name
            )

            yield job_new


@transforms.add
def handle_keyed_by_perftest(config, jobs):
    fields = ["perftest-metrics", "perftest-extra-options", "perftest-btime-variants"]
    for job in jobs:
        if job.get("perftest") is None:
            yield job
            continue

        for field in fields:
            resolve_keyed_by(job, field, item_name=job["name"])

        job.pop("perftest")
        yield job


@transforms.add
def parse_perftest_metrics(config, jobs):
    """Parse the metrics into a dictionary immediately.

    This way we can modify the extraOptions field (and others) entry through the
    transforms that come later. The metrics aren't formatted until the end of the
    transforms.
    """
    for job in jobs:
        if job.get("perftest-metrics") is None:
            yield job
            continue
        perftest_metrics = job.pop("perftest-metrics")

        # If perftest metrics is a string, split it up first
        if isinstance(perftest_metrics, list):
            new_metrics_info = [{"name": metric} for metric in perftest_metrics]
        else:
            new_metrics_info = []
            for metric, options in perftest_metrics.items():
                entry = {"name": metric}
                entry.update(options)
                new_metrics_info.append(entry)

        job["perftest-metrics"] = new_metrics_info
        yield job


@transforms.add
def split_perftest_variants(config, jobs):
    for job in jobs:
        if job.get("variants") is None:
            yield job
            continue

        for variant in job.pop("variants"):
            job_new = deepcopy(job)

            group, symbol = split_symbol(job_new["treeherder"]["symbol"])
            group += "-" + variant
            job_new["treeherder"]["symbol"] = join_symbol(group, symbol)
            job_new["name"] += "-" + variant
            job_new.setdefault("perftest-perfherder-global", {}).setdefault(
                "extraOptions", []
            ).append(variant)
            job_new[variant] = True

            yield job_new

        yield job


@transforms.add
def split_btime_variants(config, jobs):
    for job in jobs:
        if job.get("perftest-btime-variants") is None:
            yield job
            continue

        variants = job.pop("perftest-btime-variants")
        if not variants:
            yield job
            continue

        yield_existing = False
        for suffix, options in variants:
            if suffix is None:
                # Append options to the existing job
                job.setdefault("perftest-btime-variants", []).append(options)
                yield_existing = True
            else:
                job_new = deepcopy(job)
                group, symbol = split_symbol(job_new["treeherder"]["symbol"])
                symbol += "-" + suffix
                job_new["treeherder"]["symbol"] = join_symbol(group, symbol)
                job_new["name"] += "-" + suffix
                job_new.setdefault("perftest-perfherder-global", {}).setdefault(
                    "extraOptions", []
                ).append(suffix)
                # Replace the existing options with the new ones
                job_new["perftest-btime-variants"] = [options]
                yield job_new

        # The existing job has been modified so we should also return it
        if yield_existing:
            yield job


@transforms.add
def setup_http3_tests(config, jobs):
    for job in jobs:
        if job.get("http3") is None or not job.pop("http3"):
            yield job
            continue
        job.setdefault("perftest-btime-variants", []).append(
            "firefox.preference=network.http.http3.enable:true"
        )
        yield job


@transforms.add
def setup_perftest_metrics(config, jobs):
    for job in jobs:
        if job.get("perftest-metrics") is None:
            yield job
            continue
        perftest_metrics = job.pop("perftest-metrics")

        # Options to apply to each metric
        global_options = job.pop("perftest-perfherder-global", {})
        for metric_info in perftest_metrics:
            for opt, val in global_options.items():
                if isinstance(val, list) and opt in metric_info:
                    metric_info[opt].extend(val)
                elif not (isinstance(val, list) and len(val) == 0):
                    metric_info[opt] = val

        quote_escape = '\\"'
        if "win" in job.get("platform", ""):
            # Escaping is a bit different on windows platforms
            quote_escape = '\\\\\\"'

        job["run"]["command"] = job["run"]["command"].replace(
            "{perftest_metrics}",
            " ".join(
                [
                    ",".join(
                        [
                            ":".join(
                                [
                                    option,
                                    str(value)
                                    .replace(" ", "")
                                    .replace("'", quote_escape),
                                ]
                            )
                            for option, value in metric_info.items()
                        ]
                    )
                    for metric_info in perftest_metrics
                ]
            ),
        )

        yield job


@transforms.add
def setup_perftest_browsertime_variants(config, jobs):
    for job in jobs:
        if job.get("perftest-btime-variants") is None:
            yield job
            continue

        job["run"]["command"] += " --browsertime-extra-options %s" % ",".join(
            [opt.strip() for opt in job.pop("perftest-btime-variants")]
        )

        yield job


@transforms.add
def setup_perftest_extra_options(config, jobs):
    for job in jobs:
        if job.get("perftest-extra-options") is None:
            yield job
            continue
        job["run"]["command"] += " " + " ".join(job.pop("perftest-extra-options"))
        yield job


@transforms.add
def pass_perftest_options(config, jobs):
    for job in jobs:
        env = job.setdefault("worker", {}).setdefault("env", {})
        env["PERFTEST_OPTIONS"] = json.dumps(
            config.params["try_task_config"].get("perftest-options")
        )
        yield job


@transforms.add
def setup_perftest_test_date(config, jobs):
    for job in jobs:
        if (
            job.get("attributes", {}).get("batch", False)
            and "--test-date" not in job["run"]["command"]
        ):
            yesterday = (date.today() - timedelta(1)).strftime("%Y.%m.%d")
            job["run"]["command"] += " --test-date %s" % yesterday
        yield job


@transforms.add
def setup_regression_detector(config, jobs):
    for job in jobs:
        if "change-detector" in job.get("name"):
            tasks_to_analyze = []
            for task in config.params["try_task_config"].get("tasks", []):
                # Explicitly skip these tasks since they're
                # part of the mozperftest tasks
                if "side-by-side" in task:
                    continue
                if "change-detector" in task:
                    continue

                # Select these tasks
                if "browsertime" in task:
                    tasks_to_analyze.append(task)
                elif "talos" in task:
                    tasks_to_analyze.append(task)
                elif "awsy" in task:
                    tasks_to_analyze.append(task)
                elif "perftest" in task:
                    tasks_to_analyze.append(task)

            if len(tasks_to_analyze) == 0:
                yield job
                continue

            # Make the change detector task depend on the tasks to analyze.
            # This prevents the task from running until all data is available
            # within the current push.
            job["soft-dependencies"] = tasks_to_analyze
            job["requires"] = "all-completed"

            new_project = config.params["project"]
            if (
                "try" in config.params["project"]
                or config.params["try_mode"] == "try_select"
            ):
                new_project = "try"

            base_project = None
            if (
                config.params.get("try_task_config", {})
                .get("env", {})
                .get("PERF_BASE_REVISION", None)
                is not None
            ):
                task_names = " --task-name ".join(tasks_to_analyze)
                base_revision = config.params["try_task_config"]["env"][
                    "PERF_BASE_REVISION"
                ]
                base_project = new_project

                # Add all the required information to the task
                job["run"]["command"] = job["run"]["command"].format(
                    task_name=task_names,
                    base_revision=base_revision,
                    base_branch=base_project,
                    new_branch=new_project,
                    new_revision=config.params["head_rev"],
                )

        yield job


@transforms.add
def apply_perftest_tier_optimization(config, jobs):
    for job in jobs:
        job["optimization"] = {"skip-unless-backstop": None}
        job["treeherder"]["tier"] = max(job["treeherder"]["tier"], 2)
        yield job
