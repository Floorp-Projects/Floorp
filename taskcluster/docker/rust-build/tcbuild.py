#!/bin/env python
'''
This script triggers a taskcluster task, waits for it to finish,
fetches the artifacts, uploads them to tooltool, and updates
the in-tree tooltool manifests.
'''

from __future__ import print_function

import requests.packages.urllib3

import argparse
import datetime
import json
import os
import shutil
import sys
import taskcluster
import tempfile
import time
import tooltool

requests.packages.urllib3.disable_warnings()


def local_file(filename):
    '''
    Return a path to a file next to this script.
    '''
    return os.path.join(os.path.dirname(__file__), filename)


def read_tc_auth(tc_auth_file):
    '''
    Read taskcluster credentials from tc_auth_file and return them as a dict.
    '''
    return json.load(open(tc_auth_file, 'rb'))


def fill_template_dict(d, keys):
    for key, val in d.items():
        if isinstance(val, basestring) and '{' in val:
            d[key] = val.format(**keys)
        elif isinstance(val, dict):
            fill_template_dict(val, keys)


def fill_template(template_file, keys):
    '''
    Take the file object template_file, parse it as JSON, and
    interpolate (using str.template) its keys using keys.
    '''
    template = json.load(template_file)
    fill_template_dict(template, keys)
    return template


def spawn_task(queue, args):
    '''
    Spawn a Taskcluster task in queue using args.
    '''
    task_id = taskcluster.utils.slugId()
    with open(local_file('task.json'), 'rb') as template:
        keys = vars(args)
        now = datetime.datetime.utcnow()
        keys['task_created'] = now.isoformat() + 'Z'
        keys['task_deadline'] = (now + datetime.timedelta(
            hours=2)).isoformat() + 'Z'
        keys['artifacts_expires'] = (now + datetime.timedelta(
            days=1)).isoformat() + 'Z'
        payload = fill_template(template, keys)
    queue.createTask(task_id, payload)
    print('--- %s task %s submitted ---' % (now, task_id))
    return task_id


def wait_for_task(queue, task_id, initial_wait=5):
    '''
    Wait until queue reports that task task_id is completed, and return
    its run id.

    Sleep for initial_wait seconds before checking status the first time.
    Then poll periodically and print a running log of the task status.
    '''
    time.sleep(initial_wait)
    previous_state = None
    have_ticks = False
    while True:
        res = queue.status(task_id)
        state = res['status']['state']
        if state != previous_state:
            now = datetime.datetime.utcnow()
            if have_ticks:
                sys.stdout.write('\n')
                have_ticks = False
            print('--- %s task %s %s ---' % (now, task_id, state))
            previous_state = state
        if state == 'completed':
            return len(res['status']['runs']) - 1
        if state in ('failed', 'exception'):
            raise Exception('Task failed')
        sys.stdout.write('.')
        sys.stdout.flush()
        have_ticks = True
        time.sleep(10)


def fetch_artifact(queue, task_id, run_id, name, dest_dir):
    '''
    Fetch the artifact with name from task_id and run_id in queue,
    write it to a file in dest_dir, and return the path to the written
    file.
    '''
    url = queue.buildUrl('getArtifact', task_id, run_id, name)
    fn = os.path.join(dest_dir, os.path.basename(name))
    print('Fetching %s...' % name)
    try:
        r = requests.get(url, stream=True)
        r.raise_for_status()
        with open(fn, 'wb') as f:
            for chunk in r.iter_content(1024):
                f.write(chunk)
    except requests.exceptions.HTTPError:
        print('HTTP Error %d fetching %s' % (r.status_code, name))
        return None
    return fn


def make_artifact_dir(task_id, run_id):
    prefix = 'tc-artifacts.%s.%d.' % (task_id, run_id)
    print('making artifact dir %s' % prefix)
    return tempfile.mkdtemp(prefix=prefix)


def fetch_artifacts(queue, task_id, run_id):
    '''
    Fetch all artifacts from task_id and run_id in queue, write them to
    temporary files, and yield the path to each.
    '''
    try:
        tempdir = make_artifact_dir(task_id, run_id)
        res = queue.listArtifacts(task_id, run_id)
        for a in res['artifacts']:
            # Skip logs
            if a['name'].startswith('public/logs'):
                continue
            # Skip interfaces
            if a['name'].startswith('private/docker-worker'):
                continue
            yield fetch_artifact(queue, task_id, run_id, a['name'], tempdir)
    finally:
        if os.path.isdir(tempdir):
            # shutil.rmtree(tempdir)
            print('Artifacts downloaded to %s' % tempdir)
            pass


def upload_to_tooltool(tooltool_auth, task_id, artifact):
    '''
    Upload artifact to tooltool using tooltool_auth as the authentication token.
    Return the path to the generated tooltool manifest.
    '''
    try:
        oldcwd = os.getcwd()
        os.chdir(os.path.dirname(artifact))
        manifest = artifact + '.manifest'
        tooltool.main([
            'tooltool.py',
            'add',
            '--visibility=public',
            '-m', manifest,
            artifact
        ])
        tooltool.main([
            'tooltool.py',
            'upload',
            '-m', manifest,
            '--authentication-file', tooltool_auth,
            '--message', 'Built from taskcluster task {}'.format(task_id),
        ])
        return manifest
    finally:
        os.chdir(oldcwd)


def update_manifest(artifact, manifest, local_gecko_clone):
    platform = 'linux'
    manifest_dir = os.path.join(local_gecko_clone,
                                'testing', 'config', 'tooltool-manifests')
    platform_dir = [p for p in os.listdir(manifest_dir)
                    if p.startswith(platform)][0]
    tree_manifest = os.path.join(manifest_dir, platform_dir, 'releng.manifest')
    print('%s -> %s' % (manifest, tree_manifest))
    shutil.copyfile(manifest, tree_manifest)


def main():
    parser = argparse.ArgumentParser(description='Build and upload binaries')
    parser.add_argument('taskcluster_auth',
                        help='Path to a file containing Taskcluster client '
                             'ID and authentication token as a JSON file in '
                             'the form {"clientId": "...", "accessToken": "..."}')
    parser.add_argument('--tooltool-auth',
                        help='Path to a file containing a tooltool '
                             'authentication token valid for uploading files')
    parser.add_argument('--local-gecko-clone',
                        help='Path to a local Gecko clone whose tooltool '
                             'manifests will be updated with the newly-built binaries')
    parser.add_argument('--rust-branch', default='stable',
                        help='Revision of the rust repository to use')
    parser.add_argument('--task', help='Use an existing task')

    args = parser.parse_args()
    tc_auth = read_tc_auth(args.taskcluster_auth)
    queue = taskcluster.Queue({'credentials': tc_auth})
    if args.task:
        task_id, initial_wait = args.task, 0
    else:
        task_id, initial_wait = spawn_task(queue, args), 25
    run_id = wait_for_task(queue, task_id, initial_wait)
    for artifact in fetch_artifacts(queue, task_id, run_id):
        if args.tooltool_auth:
            manifest = upload_to_tooltool(args.tooltool_auth, task_id,
                                          artifact)
        if args.local_gecko_clone:
            update_manifest(artifact, manifest, args.local_gecko_clone)


if __name__ == '__main__':
    main()
