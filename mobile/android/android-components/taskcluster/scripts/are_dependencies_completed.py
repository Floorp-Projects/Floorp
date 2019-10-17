#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import taskcluster
import os


queue = taskcluster.Queue({
    'rootUrl': os.environ.get('TASKCLUSTER_PROXY_URL', 'https://taskcluster.net'),
})


def check_all_dependencies_are_completed(dependencies_task_ids):
    print('Fetching status of {} dependencies...'.format(len(dependencies_task_ids)))
    # TODO Make this dict-comprehension async once we go Python 3
    state_per_task_ids = {
        task_id: queue.status(task_id)['status']['state']
        for task_id in dependencies_task_ids
    }
    print('Statuses fetched.')
    non_completed_tasks = {
        task_id: state
        for task_id, state in state_per_task_ids.iteritems()
        if state != 'completed'
    }

    if non_completed_tasks:
        raise ValueError('Some tasks are not completed: {}'.format(non_completed_tasks))


def main():
    parser = argparse.ArgumentParser(
        description='Errors out if one of the DEPENDENCY_TASK_ID does not have the Taskcluster status "completed"'
    )

    parser.add_argument(
        'dependencies_task_ids', metavar='DEPENDENCY_TASK_ID', nargs='+',
        help="The task ID of a dependency"
    )

    result = parser.parse_args()
    check_all_dependencies_are_completed(result.dependencies_task_ids)
    print('All dependencies are completed. Reporting a green task!')
    exit(0)


if __name__ == "__main__":
    main()
