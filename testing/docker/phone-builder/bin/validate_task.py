#!/usr/bin/env python

from __future__ import print_function
import os
import os.path
import json
import urllib2
import sys
import re

def get_task(taskid):
    return json.load(urllib2.urlopen('https://queue.taskcluster.net/v1/task/' + taskid))

def check_task(task):
    payload = task['payload']

    if 'REPOSITORY' not in payload['env']:
        print('Task has no gecko repository', file=sys.stderr)
        return -1

    repo = payload['env']['REPOSITORY']
    # if it is not a mozilla repository, fail
    if not re.match(r'[a-z]+://hg\.mozilla\.org', repo):
        print('Invalid repository', repo, file=sys.stderr)
        return -1

    if 'artifacts' in payload:
        artifacts = payload['artifacts']
        # If any of the artifacts makes reference to 'public',
        # abort the task
        if any(map(lambda a: 'public' in a, artifacts)):
            print('Cannot upload to public', file=sys.stderr)
            return -1

    return 0

def main():
    taskid = os.getenv('TASK_ID')

    # If the task id is None, we assume we are running docker locally
    if taskid is None:
        sys.exit(0)

    task = get_task(taskid)
    sys.exit(check_task(task))

if __name__ == '__main__':
    main()
