# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import shutil
import tempfile
from pathlib import Path


class PerformanceChangeDetected(Exception):
    """Raised when a performance change is detected.

    This failure happens with regressions, and improvements. There
    is no unique failure for each of them.

    TODO: We eventually need to be able to distinguish between these.
    To do so, we would need to incorporate the "lower_is_better" settings
    into the detection tooling.
    """

    pass


def run_side_by_side(artifacts, kwargs):
    from mozperftest_tools.side_by_side import SideBySide

    output_specified = None
    if "output" in kwargs:
        output_specified = kwargs.pop("output")

    if output_specified:
        s = SideBySide(str(output_specified))
        s.run(**kwargs)
        print(f"Results can be found in {output_specified}")
    else:
        tempdir = tempfile.mkdtemp()
        s = SideBySide(str(tempdir))
        s.run(**kwargs)
        try:
            for file in os.listdir(tempdir):
                if (
                    file.endswith(".mp4")
                    or file.endswith(".gif")
                    or file.endswith(".json")
                ):
                    print(f"Copying from {tempdir}/{file} to {artifacts}")
                    shutil.copy(Path(tempdir, file), artifacts)
        finally:
            shutil.rmtree(tempdir)


def _gather_task_names(kwargs):
    task_names = kwargs.get("task_names", [])
    if len(task_names) == 0:
        if kwargs.get("test_name", None) is None:
            raise Exception("No test, or task names given!")
        if kwargs.get("platform", None) is None:
            raise Exception("No platform, or task names given!")
        task_names.append(kwargs["platform"] + "-" + kwargs["test_name"])
    return task_names


def _get_task_splitter(task):
    splitter = "/opt-"
    if splitter not in task:
        splitter = "/" + task.split("/")[-1].split("-")[0] + "-"
    return splitter


def _format_changes_to_str(all_results):
    changes_detected = None
    for task, results in all_results.items():
        for pltype, metrics in results["metrics-with-changes"].items():
            for metric, changes in metrics.items():
                for revision, diffs in changes.items():
                    if changes_detected is None:
                        changes_detected = "REVISION  PL_TYPE  METRIC %-DIFFERENCE\n"
                    changes_detected += f"{revision} {pltype} {metric} {str(diffs)}\n"
    return changes_detected


def run_change_detector(artifacts, kwargs):
    from mozperftest_tools.regression_detector import ChangeDetector

    tempdir = tempfile.mkdtemp()
    detector = ChangeDetector(tempdir)

    all_results = {}
    results_path = Path(artifacts, "results.json")
    try:
        for task in _gather_task_names(kwargs):
            splitter = _get_task_splitter(task)

            platform, test_name = task.split(splitter)
            platform += splitter[:-1]

            new_test_name = test_name
            new_platform_name = platform
            if kwargs["new_test_name"] is not None:
                new_test_name = kwargs["new_test_name"]
            if kwargs["new_platform"] is not None:
                new_platform_name = kwargs["new_platform_name"]

            all_changed_revisions, changed_metric_revisions = detector.detect_changes(
                test_name=test_name,
                new_test_name=new_test_name,
                platform=platform,
                new_platform=new_platform_name,
                base_revision=kwargs["base_revision"],
                new_revision=kwargs["new_revision"],
                base_branch=kwargs["base_branch"],
                new_branch=kwargs["new_branch"],
                # Depth of -1 means auto-computed (everything in between the two given revisions),
                # None is direct comparisons, anything else uses the new_revision as a start
                # and goes backwards from there.
                depth=kwargs.get("depth", None),
                skip_download=False,
                overwrite=False,
            )

            # The task names are unique, so we don't need to worry about
            # them overwriting each other
            all_results[task] = {}
            all_results[task]["revisions-with-changes"] = list(all_changed_revisions)
            all_results[task]["metrics-with-changes"] = changed_metric_revisions

        changes_detected = _format_changes_to_str(all_results)
        if changes_detected is not None:
            print(changes_detected)
            raise PerformanceChangeDetected(
                "[ERROR] A significant performance change was detected in your patch! "
                "See the logs above, or the results.json artifact that was produced for "
                "more information."
            )

    finally:
        shutil.rmtree(tempdir)

        print(f"Saving change detection results to {str(results_path)}")
        with results_path.open("w") as f:
            json.dump(all_results, f, indent=4)


TOOL_RUNNERS = {
    "side-by-side": run_side_by_side,
    "change-detector": run_change_detector,
}
