# SPDX-License-Identifier: MPL-2.0
"""Initialize a single fresh Firefox45 job from normal authenticated API lineage."""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import re
import stat
import time
import urllib.error
import urllib.request

from qa_core import Failure, require

REPOSITORY = 'Floorp-Projects/Floorp-Runtime'
BRANCH = 'codex/mac-native-firefox45-fresh-recovery64-20261020'
JOB_NAME = 'fresh-firefox45'
PROFILE = 'fresh-firefox45-job-origin-v1'
CONTINUATION_BRANCH = 'b/qa-fresh45-20261026'
CONTINUATION_PROFILE = 'fresh-firefox45-job-origin-20261026-v1'
API_CAP = 2 * 1024**2


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, newurl):
        return None


def _decode(raw):
    def pairs(rows):
        value = {}
        for key, item in rows:
            require(key not in value, 'fresh-origin-api-json')
            value[key] = item
        return value
    try:
        return json.loads(raw, object_pairs_hook=pairs,
            parse_constant=lambda _: require(False, 'fresh-origin-api-json'))
    except (ValueError, UnicodeError, RecursionError):
        raise Failure('fresh-origin-api-json') from None


def initialize(task, *, environment=None, opener=None, wall_clock=None, monotonic_clock=None,
               continuation=False):
    """No clock reset, checkpoint role, payload operation, or permission mutation."""
    env = os.environ if environment is None else environment
    wall = time.time if wall_clock is None else wall_clock
    mono = time.monotonic if monotonic_clock is None else monotonic_clock
    require(type(continuation) is bool, 'fresh-origin-run-scope')
    branch = CONTINUATION_BRANCH if continuation else BRANCH
    profile = CONTINUATION_PROFILE if continuation else PROFILE
    require(env.get('GITHUB_REPOSITORY') == REPOSITORY and env.get('GITHUB_EVENT_NAME') == 'push' and
            env.get('GITHUB_RUN_ATTEMPT') == '1' and env.get('GITHUB_REF') == 'refs/heads/' + branch and
            env.get('FRESH_PUSH_CREATED') == 'true' and env.get('FRESH_PUSH_BEFORE') == '0' * 40,
            'fresh-origin-run-scope')
    run_id, head, token = env.get('GITHUB_RUN_ID'), env.get('GITHUB_SHA'), env.get('JOB_READ_TOKEN')
    require(type(run_id) is str and re.fullmatch(r'[1-9][0-9]{0,19}', run_id) is not None and
            type(head) is str and re.fullmatch(r'[0-9a-f]{40}', head) is not None and
            type(token) is str and 0 < len(token) <= 16384, 'fresh-origin-environment')
    start_mono = mono()
    require(type(start_mono) in (int, float) and math.isfinite(start_mono) and start_mono > 0,
            'fresh-origin-clock')
    deadline = start_mono + 90
    client = urllib.request.build_opener(NoRedirect) if opener is None else opener
    base = 'https://api.github.com/repos/' + REPOSITORY + '/actions/'
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}

    def within_api_deadline():
        now = mono()
        return type(now) in (int, float) and math.isfinite(now) and start_mono <= now < deadline

    def api(suffix):
        require(within_api_deadline(), 'fresh-origin-api-deadline')
        try:
            with client.open(urllib.request.Request(base + suffix, headers=headers), timeout=30) as response:
                require(type(response.status) is int and response.status == 200, 'fresh-origin-http')
                raw = response.read(API_CAP + 1)
        except (urllib.error.URLError, OSError):
            raise Failure('fresh-origin-http') from None
        require(type(raw) is bytes and len(raw) <= API_CAP, 'fresh-origin-api-size')
        require(within_api_deadline(), 'fresh-origin-api-deadline')
        return _decode(raw)

    run = api('runs/' + run_id)
    require(type(run) is dict and type(run.get('id')) is int and run['id'] == int(run_id) and
            run.get('head_sha') == head and run.get('head_branch') == branch and
            type(run.get('run_attempt')) is int and run['run_attempt'] == 1 and run.get('event') == 'push' and run.get('status') == 'in_progress' and
            type(run.get('repository')) is dict and run['repository'].get('full_name') == REPOSITORY,
            'fresh-origin-run-provenance')
    jobs = api('runs/' + run_id + '/jobs')
    require(type(jobs) is dict and type(jobs.get('total_count')) is int and jobs['total_count'] == 1 and
            type(jobs.get('jobs')) is list and len(jobs['jobs']) == 1, 'fresh-origin-job-count')
    job = jobs['jobs'][0]
    require(type(job) is dict and type(job.get('id')) is int and job['id'] > 0 and
            type(job.get('run_id')) is int and job['run_id'] == int(run_id) and
            type(job.get('run_attempt')) is int and job['run_attempt'] == 1 and job.get('head_sha') == head and
            job.get('name') == JOB_NAME and job.get('status') == 'in_progress' and
            job.get('labels') == ['macos-15-intel'] and type(job.get('started_at')) is str and
            re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', job['started_at']) is not None,
            'fresh-origin-current-job')
    try:
        started = datetime.datetime.fromisoformat(job['started_at'].replace('Z', '+00:00')).timestamp()
    except (ValueError, OverflowError):
        raise Failure('fresh-origin-started-at') from None
    now_wall, now_mono = wall(), mono()
    require(all(type(value) in (int, float) and math.isfinite(value) for value in
                (started, now_wall, now_mono)) and start_mono <= now_mono < deadline, 'fresh-origin-clock')
    age = now_wall - started
    origin = now_mono - max(0, age)
    require(-2 <= age < 600 and 0 < origin <= now_mono, 'fresh-origin-job-age')
    record = {'schema': 1, 'profile': profile, 'repository': REPOSITORY, 'run': int(run_id),
              'attempt': 1, 'head': head, 'ref': env['GITHUB_REF'], 'event': 'push',
              'job': job['id'], 'jobName': JOB_NAME, 'startedAt': job['started_at'],
              'platformMinutes': 360, 'workDeadlineMinutes': 345, 'clockResetAllowed': False}
    supplied = Path(task)
    require(not supplied.is_symlink(), 'fresh-origin-task-exists-or-unavailable')
    task = supplied.resolve()
    try:
        task.mkdir(mode=0o700, exist_ok=False)
    except OSError:
        raise Failure('fresh-origin-task-exists-or-unavailable') from None
    metadata = task.lstat()
    require(stat.S_ISDIR(metadata.st_mode) and metadata.st_uid == os.geteuid() and
            stat.S_IMODE(metadata.st_mode) == 0o700, 'fresh-origin-task-binding')
    try:
        for name, raw in (('started', str(started).encode()), ('started-monotonic', str(origin).encode()),
                          ('job-origin.json', (json.dumps(record, sort_keys=True, indent=2) + '\n').encode())):
            descriptor = os.open(task / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            try:
                with os.fdopen(descriptor, 'wb') as output:
                    descriptor = None
                    output.write(raw)
            finally:
                if descriptor is not None:
                    os.close(descriptor)
    except OSError:
        # Leave an incomplete private task as failed evidence; never reset/reuse it.
        raise Failure('fresh-origin-write') from None
    return record


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--continuation-20261026', action='store_true')
    args = parser.parse_args()
    try:
        initialize(args.task, continuation=args.continuation_20261026)
    except Failure as error:
        print(json.dumps({'freshJobOrigin': 'failed', 'code': str(error)}), flush=True)
        return 1
    except BaseException:
        print(json.dumps({'freshJobOrigin': 'failed', 'code': 'fresh-origin-unavailable'}), flush=True)
        return 1
    print(json.dumps({'freshJobOrigin': 'initialized', 'schema': 1, 'clockResetAllowed': False}), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
