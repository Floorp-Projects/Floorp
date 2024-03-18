#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os

import taskcluster

queue = taskcluster.Queue(
    {
        "rootUrl": os.environ.get("TASKCLUSTER_PROXY_URL", "https://taskcluster.net"),
    }
)


def check_all_dependencies_are_completed(current_task_id):
    print(f"Fetching task definition of {current_task_id}...")
    task = queue.task(current_task_id)
    dependencies_task_ids = task["dependencies"]

    print(f"Fetching status of {len(dependencies_task_ids)} dependencies...")
    # TODO Make this dict-comprehension async once we go Python 3
    state_per_task_ids = {
        task_id: queue.status(task_id)["status"]["state"]
        for task_id in dependencies_task_ids
    }
    print("Statuses fetched.")
    non_completed_tasks = {
        task_id: state
        for task_id, state in state_per_task_ids.items()
        if state != "completed"
    }

    if non_completed_tasks:
        raise ValueError(f"Some tasks are not completed: {non_completed_tasks}")


def main():
    parser = argparse.ArgumentParser(
        description='Errors out if one of the DEPENDENCY_TASK_ID does not have the Taskcluster status "completed"'
    )

    parser.add_argument(
        "current_task_id",
        metavar="CURRENT_TASK_ID",
        help="The task ID of the current running task",
    )

    result = parser.parse_args()
    check_all_dependencies_are_completed(result.current_task_id)
    print("All dependencies are completed. Reporting a green task!")
    exit(0)


if __name__ == "__main__":
    main()
