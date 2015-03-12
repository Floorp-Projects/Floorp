#!/usr/bin/env python

from __future__ import print_function
import os
import os.path
import json
import urllib2
import sys
import re
import subprocess

repo_matcher = re.compile(r'[a-z]+://(hg|git)\.mozilla\.org')
image_matcher = re.compile(r'^https:\/\/queue\.taskcluster\.net\/v1\/task\/.+\/artifacts\/private\/build\/flame-kk\.zip$')

def get_task(taskid):
    return json.load(urllib2.urlopen('https://queue.taskcluster.net/v1/task/' + taskid))

def check_task(task):
    payload = task['payload']

    if 'DEVICE_CAPABILITIES' not in payload['env']:
        print('Device capalities are required.', file=sys.stderr)
        return -1

    capabilities = json.loads(payload['env']['DEVICE_CAPABILITIES'])

    if 'build' not in capabilities:
        print('Build image url is required', file=sys.stderr)
        return -1

    image = capabilities['build']

    if not image_matcher.match(image):
        print('Invalid image url', file=sys.stderr)
        return -1

    if 'GAIA_HEAD_REPOSITORY' not in payload['env']:
        print('Task has no head gaia repository', file=sys.stderr)
        return -1

    repo = payload['env']['GAIA_HEAD_REPOSITORY']
    # if it is not a mozilla repository, fail
    if not repo_matcher.match(repo):
        print('Invalid head repository', repo, file=sys.stderr)
        return -1

    if 'GAIA_BASE_REPOSITORY' not in payload['env']:
        print('Task has no base gaia repository', file=sys.stderr)
        return -1

    repo = payload['env']['GAIA_BASE_REPOSITORY']
    if not repo_matcher.match(repo):
        print('Invalid base repository', repo, file=sys.stderr)
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
    if taskid is not None:
        task = get_task(taskid)
        sys.exit(check_task(task))

if __name__ == '__main__':
    main()
