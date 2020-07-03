#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import pathlib
import json
import urllib.request
import re
import subprocess
import sys


class Logger:
    @classmethod
    def info(cls, s):
        print('[INFO]', s)

        # Flush to make it apeear immediately in automation log.
        sys.stdout.flush()

    @classmethod
    def fetch(cls, url):
        cls.info(f'Fetching {url}')

    @classmethod
    def cmd(cls, cmd):
        def format_cmd(s):
            if ' ' in s:
                escaped = s.replace('"', '\"')
                return f'"{escaped}"'
            return s

        formatted_command = ' '.join(list(map(format_cmd, cmd)))
        cls.info(f'$ {formatted_command}')


class GitRepository:
    def __init__(self, path):
        self.path = path

        self.git_dir = self.path / '.git'
        if not self.git_dir.exists():
            print(f'{self.path} is not a Git repository.', file=sys.stderr)
            sys.exit(1)

    def get_output(self, *args):
        cmd = ['git'] + list(args)
        Logger.cmd(cmd)
        output = subprocess.run(cmd,
                                capture_output=True,
                                cwd=self.path)

        return output.stdout.decode()

    def run(self, *args):
        cmd = ['git'] + list(args)
        Logger.cmd(cmd)
        subprocess.run(cmd,
                       check=True,
                       cwd=self.path)

    def commit_message(self, rev):
        return self.get_output('log', '-1', '--pretty=format:%s%n', rev)


class MCRemoteRepository:
    HG_API_URL = 'https://hg.mozilla.org/mozilla-central/'

    @classmethod
    def call(cls, name, path):
        url = f'{cls.HG_API_URL}{name}{path}'
        Logger.fetch(url)
        req = urllib.request.Request(url, None, {})
        response = urllib.request.urlopen(req)
        return response.read()

    @classmethod
    def call_json(cls, name, path):
        return json.loads(cls.call(name, path))

    @classmethod
    def file(cls, rev, path):
        return cls.call('raw-file', f'/{rev}{path}')


class TreeHerder:
    API_URL = 'https://treeherder.mozilla.org/api/'

    @classmethod
    def call(cls, name):
        url = f'{cls.API_URL}{name}'
        Logger.fetch(url)
        req = urllib.request.Request(url, None, {
            'User-Agent': 'smoosh-tools',
        })
        response = urllib.request.urlopen(req)
        return response.read()

    @classmethod
    def call_json(cls, name):
        return json.loads(cls.call(name))

    @classmethod
    def push_id(cls, rev):
        push = cls.call_json(f'project/mozilla-central/push/?full=true&format=json&count=1&revision={rev}')
        return push['results'][0]['id']

    @classmethod
    def jobs(cls, push_id):
        push = cls.call_json(f'jobs/?push_id={push_id}&format=json')
        count = push['count']
        results = []
        results += push['results']

        page = 2
        while len(results) < count:
            push = cls.call_json(f'jobs/?push_id={push_id}&format=json&page={page}')
            results += push['results']
            page += 1

        return results


class Status:
    def run(is_ci):
        Logger.info('Fetching ci_generated branch')

        jsparagus = GitRepository(pathlib.Path('./'))
        jsparagus.run('fetch', 'origin', 'ci_generated')

        Logger.info('Checking mozilla-central tip revision')

        m_c_rev = MCRemoteRepository.call_json('json-log', '/tip/')['node']
        cargo_file = MCRemoteRepository.file(
            m_c_rev,
            '/js/src/frontend/smoosh/Cargo.toml'
        ).decode()
        m = re.search('rev = "(.+)"', cargo_file)
        ci_generated_rev = m.group(1)

        Logger.info('Checking jsparagus referred by mozilla-central')

        message = jsparagus.commit_message(ci_generated_rev)
        m = re.search('for ([A-Fa-f0-9]+)', message)
        master_rev = m.group(1)

        Logger.info('Checking build status')

        push_id = TreeHerder.push_id(m_c_rev)
        jobs = TreeHerder.jobs(push_id)
        nonunified_job = None
        smoosh_job = None
        for job in jobs:
            if 'spidermonkey-sm-nonunified-linux64/debug' in job:
                nonunified_job = job
            if 'spidermonkey-sm-smoosh-linux64/debug' in job:
                smoosh_job = job

        def get_result(job):
            if job:
                if 'completed' in job:
                    if 'success' in job:
                        return 'OK'
                    else:
                        return 'NG'
                else:
                    return 'not yet finished'
            else:
                return 'unknown'

        nonunified_result = get_result(nonunified_job)
        smoosh_result = get_result(smoosh_job)

        if is_ci:
            print(f'##[set-output name=mc;]{m_c_rev}')
            print(f'##[set-output name=jsparagus;]{master_rev}')
            print(f'##[set-output name=build;]{nonunified_result}')
            print(f'##[set-output name=test;]{smoosh_result}')
        else:
            print(f'mozilla-central tip: {m_c_rev}')
            print(f'referred jsparagus revision: {master_rev}')
            print(f'Build status:')
            print(f'  Build with --enable-smoosh: {nonunified_result}')
            print(f'  Test with --smoosh: {smoosh_result}')


is_ci = False
if len(sys.argv) > 1:
    if sys.argv[1] == 'ci':
        is_ci = True

Status.run(is_ci)
