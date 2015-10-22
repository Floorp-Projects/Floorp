#!/usr/bin/env python

from __future__ import print_function
import os
import json
import urllib2
import sys
import re
import subprocess
import shutil

HOME = os.getenv('HOME')

WORKSPACE = os.path.join(HOME, 'workspace')
ARTIFACTS_PUBLIC = os.path.normpath(os.path.join(HOME, 'artifacts-public'))
SCRIPTS_PATH = os.path.join(HOME, 'bin')
COMMANDS = {
    'phone': 'build-phone.sh',
    'phone-ota': 'build-phone-ota.sh',
    'dolphin': 'build-dolphin.sh'
}

# Be careful when adding username containing '.', as it expands to any character
# in regular expressions. It must be escaped properly:
#  right: r'foo\.bar',
#  wrong: 'foo.bar' # matches fooabar, foo1bar, foo_bar, etc

github_allowed_accounts = [
    'walac',
    'nhirata',
    'selenamarie',
    'ShakoHo',
]

bitbucket_allowed_accounts = [
    'walac',
    'selenamarie',
]

def build_repo_matcher():
    github_expr = r'(github\.com/(' + '|'.join(github_allowed_accounts) + ')/)'
    bitbucket_expr = r'(bitbucket\.org/(' + '|'.join(bitbucket_allowed_accounts) + ')/)'
    mozilla_expr = r'((hg|git)\.mozilla\.org)'
    expr = r'^https?://(' + '|'.join((github_expr, bitbucket_expr, mozilla_expr)) + ')'
    return re.compile(expr)

repo_matcher = build_repo_matcher()

def get_task(taskid):
    return json.load(urllib2.urlopen('https://queue.taskcluster.net/v1/task/' + taskid))

def check_repo(repo):
    if not repo_matcher.match(repo):
        print('Invalid repository "{}"'.format(repo), file=sys.stderr)
        return -1
    return 0

# Cleanup artifacts and known credentials. This is to avoid a malicious
# task to map a directory containing sensible files expose secret files.
def cleanup(task):
    payload = task['payload']
    for key, value in payload.get('artifacts', {}).items():
        shutil.rmtree(value['path'], ignore_errors=True)

    shutil.rmtree(os.path.join(HOME, '.aws'), ignore_errors=True)

    try:
        os.remove(os.path.join(HOME, 'socorro.token'))
    except (IOError, OSError):
        pass

def check_task(task):
    repositories_to_check = [
        'GECKO_HEAD_REPOSITORY',
        'GECKO_BASE_REPOSITORY',
    ]

    payload = task['payload']

    for repo in repositories_to_check:
        if repo not in payload['env']:
            print('Repository {} is not in payload.env.'.format(repo), file=sys.stderr)
            return -1
        ret = check_repo(payload['env'][repo])
        if ret != 0:
            return ret

    for key, value in payload.get('artifacts', {}).items():
        if key.startswith('public') and \
                os.path.normpath(value['path']) != ARTIFACTS_PUBLIC:
            print('{} cannot be a public artifact.'.format(value['path']),
                  file=sys.stderr)
            return -1

    if sys.argv[1] not in COMMANDS:
        print("Invalid build command '{}', valid commands are '{}'".format(sys.argv[1], ", ".join(COMMANDS.keys())))
        return -1

    return 0

def run():
    command = COMMANDS[sys.argv[1]]

    checkout_gecko = ['checkout-gecko', WORKSPACE]
    cd_scripts = ['cd', SCRIPTS_PATH]
    build = ['buildbot_step', '"Build"', os.path.join(SCRIPTS_PATH, command), WORKSPACE]
    and_ = ['&&']
    command = ' '.join(checkout_gecko + and_ + cd_scripts + and_ + build)

    try:
        return subprocess.call(command, shell=True)
    except subprocess.CalledProcessError as e:
        return e.returncode

def main():
    taskid = os.getenv('TASK_ID')

    # If the task id is None, we assume we are running docker locally
    if taskid is not None:
        task = get_task(taskid)
        ret = check_task(task)
        if ret != 0:
            cleanup(task)
            return ret

    if len(sys.argv) > 1:
        return run()


if __name__ == '__main__':
    sys.exit(main())
