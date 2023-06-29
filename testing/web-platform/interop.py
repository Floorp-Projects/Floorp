# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import math
import os
import re
import shutil
import sys
import tempfile
from typing import Callable, Iterable, List, Mapping, Optional, Tuple

repos = ["autoland", "mozilla-central", "try", "mozilla-central", "mozilla-beta", "wpt"]

default_fetch_task_filters = ["-web-platform-tests-|-spidermonkey-"]
default_interop_task_filters = {
    "wpt": ["-firefox-"],
    None: [
        "web-platform-tests",
        "linux.*-64",
        "/opt",
        "!-nofis|-headless|-asan|-tsan|-ccov",
    ],
}


def get_parser_fetch_logs() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--log-dir", action="store", help="Directory into which to download logs"
    )
    parser.add_argument(
        "--task-filter",
        dest="task_filters",
        action="append",
        help="Regex filter applied to task names. Filters starting ! must not match. Filters starting ^ (after any !) match the entire task name, otherwise any substring can match. Multiple filters must all match",
    )
    parser.add_argument(
        "--check-complete",
        action="store_true",
        help="Only download logs if the task is complete",
    )
    parser.add_argument(
        "commits",
        nargs="+",
        help="repo:commit e.g. mozilla-central:fae24810aef1 for the runs to include",
    )
    return parser


def get_parser_interop_score() -> argparse.Namespace:
    parser = get_parser_fetch_logs()
    parser.add_argument(
        "--year",
        action="store",
        default=2023,
        type=int,
        help="Interop year to score against",
    )
    parser.add_argument(
        "--category-filter",
        action="append",
        dest="category_filters",
        help="Regex filter applied to category names. Filters starting ! must not match. Filters starting ^ (after any !) match the entire task name, otherwise any substring can match. Multiple filters must all match",
    )
    return parser


def print_scores(
    runs: Iterable[Tuple[str, str]],
    results_by_category: Mapping[str, int],
    include_total: bool,
):
    tab = "\t"  # For f-string
    header = "\t".join(f"{repo}:{commit}" for repo, commit in runs)
    print(f"\t{header}")
    totals = [0] * len(runs)
    for category, category_results in results_by_category.items():
        for i, result in enumerate(category_results):
            totals[i] += result
        print(f"{category}\t{tab.join(str(item / 10) for item in category_results)}")
    if include_total:
        totals = [math.floor(float(item) / len(results_by_category)) for item in totals]
        print(f"Total\t{tab.join(str(item / 10) for item in totals)}")


def get_wptreports(
    repo: str, commit: str, task_filters: List[str], log_dir: str, check_complete: bool
) -> List[str]:
    import tcfetch

    return tcfetch.download_artifacts(
        repo,
        commit,
        task_filters=task_filters,
        check_complete=check_complete,
        out_dir=log_dir,
    )


def get_runs(commits: List[str]) -> List[Tuple[str, str]]:
    runs = []
    for item in commits:
        if ":" not in item:
            raise ValueError(f"Expected commits of the form repo:commit, got {item}")
        repo, commit = item.split(":", 1)
        if repo not in repos:
            raise ValueError(f"Unsupported repo {repo}")
        runs.append((repo, commit))
    return runs


def get_category_filter(
    category_filters: Optional[List[str]],
) -> Optional[Callable[[str], bool]]:
    if category_filters is None:
        return None

    filters = []
    for item in category_filters:
        if not item:
            continue
        invert = item[0] == "!"
        if invert:
            item = item[1:]
        if item[0] == "^":
            regex = re.compile(item)
        else:
            regex = re.compile(f"^(.*)(?:{item})")
        filters.append((regex, invert))

    def match_filters(category):
        for regex, invert in filters:
            matches = regex.match(category) is not None
            if invert:
                matches = not matches
            if not matches:
                return False
        return True

    return match_filters


def fetch_logs(
    commits: List[str],
    task_filters: List[str],
    log_dir: Optional[str],
    check_complete: bool,
    **kwargs,
):
    runs = get_runs(commits)

    if not task_filters:
        task_filters = default_fetch_task_filters

    if log_dir is None:
        log_dir = os.path.abspath(os.curdir)

    for repo, commit in runs:
        get_wptreports(repo, commit, task_filters, log_dir, check_complete)


def score_runs(
    commits: List[str],
    task_filters: List[str],
    log_dir: Optional[str],
    year: int,
    check_complete: bool,
    category_filters: Optional[List[str]],
    **kwargs,
):
    from wpt_interop import score

    runs = get_runs(commits)

    temp_dir = None
    if log_dir is None:
        temp_dir = tempfile.mkdtemp()
        log_dir = temp_dir

    try:
        run_logs = []
        for repo, commit in runs:
            if not task_filters:
                if repo in default_interop_task_filters:
                    filters = default_interop_task_filters[repo]
                else:
                    filters = default_interop_task_filters[None]
            else:
                filters = task_filters

            log_paths = get_wptreports(repo, commit, filters, log_dir, check_complete)
            if not log_paths:
                print(f"Failed to get any logs for {repo}:{commit}", file=sys.stderr)
            else:
                run_logs.append(log_paths)

        if not run_logs:
            print("No logs to process", file=sys.stderr)

        include_total = category_filters is None

        category_filter = (
            get_category_filter(category_filters) if category_filters else None
        )

        scores = score.score_wptreports(
            run_logs, year=year, category_filter=category_filter
        )
        print_scores(runs, scores, include_total)
    finally:
        if temp_dir is not None:
            shutil.rmtree(temp_dir, True)
