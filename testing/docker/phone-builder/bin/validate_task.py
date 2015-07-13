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

def get_task(taskid):
    return json.load(urllib2.urlopen('https://queue.taskcluster.net/v1/task/' + taskid))

def check_task(task):
    payload = task['payload']

    if 'GECKO_HEAD_REPOSITORY' not in payload['env']:
        print('Task has no head gecko repository', file=sys.stderr)
        return -1

    repo = payload['env']['GECKO_HEAD_REPOSITORY']
    # if it is not a mozilla repository, fail
    if not repo_matcher.match(repo):
        print('Invalid head repository', repo, file=sys.stderr)
        return -1

    if 'GECKO_BASE_REPOSITORY' not in payload['env']:
        print('Task has no base gecko repository', file=sys.stderr)
        return -1

    repo = payload['env']['GECKO_BASE_REPOSITORY']
    if not repo_matcher.match(repo):
        print('Invalid base repository', repo, file=sys.stderr)
        return -1

    if "MOZHARNESS_CONFIG" in payload["env"] and \
            "blobfree" in payload["env"]["MOZHARNESS_CONFIG"]:
        return 0

    locations = task["extra"]["locations"]
    if "img" in locations:
        img = locations["img"]
        if img.startswith("public"):
            print('Cannot upload images to public', file=sys.stderr)
            return -1

    return 0

def main():
    taskid = os.getenv('TASK_ID')

    # If the task id is None, we assume we are running docker locally
    if taskid is not None:
        task = get_task(taskid)
        ret = check_task(task)
        if ret != 0:
            return ret

    if len(sys.argv) > 1:
        try:
            return subprocess.call(sys.argv[1:], shell=True)
        except subprocess.CalledProcessError as e:
            return e.returncode

    return 0

if __name__ == '__main__':
    sys.exit(main())
