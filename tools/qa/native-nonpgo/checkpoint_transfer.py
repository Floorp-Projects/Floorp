# SPDX-License-Identifier: MPL-2.0
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import time
import urllib.error
import urllib.request

from fetch_sdk import NoRedirect
from support import HTTPSRedirect, digest, require, write_json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=('init', 'fetch', 'confirm'))
    parser.add_argument('--role', choices=('producer', 'consumer'), required=True)
    parser.add_argument('--task', type=Path, required=True)
    args = parser.parse_args()
    plan = json.loads(Path(__file__).with_name('plan.json').read_text())
    spec = plan['checkpoint']
    require(args.mode == 'init' or args.role == ('producer' if args.mode == 'confirm' else 'consumer'),
            'checkpoint-transfer-role')
    deadline = (time.monotonic() + 90 if args.mode == 'init' else
                float((args.task / 'started-monotonic').read_text()) +
                (345 if args.mode == 'confirm' else 40) * 60)
    require(os.environ['GITHUB_REPOSITORY'] == 'Floorp-Projects/Floorp-Runtime' and
            os.environ['GITHUB_RUN_ATTEMPT'] == '1' and os.environ['GITHUB_EVENT_NAME'] == 'push',
            'checkpoint-run-scope')
    run_id = os.environ['GITHUB_RUN_ID']
    require(run_id.isdecimal(), 'checkpoint-run-id')
    base = 'https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/actions/'
    headers = {'Authorization': 'Bearer ' + os.environ['CHECKPOINT_READ_TOKEN'],
               'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'}
    opener = urllib.request.build_opener(NoRedirect)
    def api(suffix):
        require(time.monotonic() < deadline, 'checkpoint-api-deadline')
        with opener.open(urllib.request.Request(base + suffix, headers=headers), timeout=30) as response:
            value = response.read(2 * 1024**2 + 1)
        require(len(value) <= 2 * 1024**2, 'checkpoint-api-size')
        require(time.monotonic() < deadline, 'checkpoint-api-deadline')
        return json.loads(value)
    run = api('runs/' + run_id)
    require(run['head_sha'] == os.environ['GITHUB_SHA'] and run['run_attempt'] == 1 and
            run['event'] == 'push', 'checkpoint-run-provenance')
    jobs = api('runs/' + run_id + '/jobs')
    require(jobs['total_count'] <= 2, 'checkpoint-job-count')
    if args.mode == 'init':
        selected = [row for row in jobs['jobs'] if row['name'] == 'checkpoint-' + args.role and
                    row['status'] == 'in_progress']
        require(len(selected) == 1 and selected[0]['labels'] == ['macos-15-intel'], 'checkpoint-current-job')
        start = datetime.datetime.fromisoformat(selected[0]['started_at'].replace('Z', '+00:00')).timestamp()
        age = time.time() - start
        require(-2 <= age < 600, 'checkpoint-job-origin')
        args.task.mkdir(mode=0o700, exist_ok=False)
        (args.task / 'started').write_text(str(start))
        (args.task / 'started-monotonic').write_text(str(time.monotonic() - max(0, age)))
        write_json(args.task / 'job-origin.json', {'job': selected[0]['id'], 'startedAt': selected[0]['started_at'],
                                                 'run': run_id, 'role': args.role})
        return
    artifact_id = os.environ['CHECKPOINT_ARTIFACT_ID']
    require(artifact_id.isdecimal(), 'checkpoint-artifact-id')
    artifact = api('artifacts/' + artifact_id)
    require(artifact['id'] == int(artifact_id) and artifact['workflow_run']['id'] == int(run_id) and
            artifact['name'] == spec['artifactName'] and not artifact['expired'] and
            0 < artifact['size_in_bytes'] <= spec['maxStoredBytes'], 'checkpoint-artifact-provenance')
    if args.mode == 'confirm':
        ready = json.loads((args.task / 'checkpoint-ready.json').read_text())
        require(artifact['digest'] == 'sha256:' + ready['sha256'] and artifact['size_in_bytes'] == ready['bytes'] and
                os.environ['CHECKPOINT_ACTION_DIGEST'] == ready['sha256'], 'checkpoint-service-integrity')
        created = datetime.datetime.fromisoformat(artifact['created_at'].replace('Z', '+00:00'))
        expires = datetime.datetime.fromisoformat(artifact['expires_at'].replace('Z', '+00:00'))
        require(0 < (expires - created).total_seconds() <= 86400 + 120, 'checkpoint-retention')
        require(api('runs/' + run_id + '/artifacts')['total_count'] == 1, 'checkpoint-artifact-count')
        value = {'id': artifact['id'], 'run': run_id, 'bytes': ready['bytes'], 'sha256': ready['sha256'],
                 'manifestSHA256': ready['manifestSHA256'], 'createdAt': artifact['created_at'],
                 'expiresAt': artifact['expires_at'], 'rawSingleFile': True, 'retentionDays': 1}
        write_json(args.task / 'checkpoint-service.json', value)
        print(json.dumps({'checkpointArtifact': value}), flush=True)
        return
    producer = [row for row in jobs['jobs'] if row['name'] == 'checkpoint-producer']
    require(len(producer) == 1 and producer[0]['conclusion'] == 'success', 'checkpoint-producer-job')
    checksum = os.environ['CHECKPOINT_ARCHIVE_SHA256']
    manifest_hash = os.environ['CHECKPOINT_MANIFEST_SHA256']
    size = os.environ['CHECKPOINT_ARCHIVE_BYTES']
    require(re.fullmatch('[0-9a-f]{64}', checksum) and re.fullmatch('[0-9a-f]{64}', manifest_hash) and
            size.isdecimal() and int(size) == artifact['size_in_bytes'] and
            artifact['digest'] == 'sha256:' + checksum, 'checkpoint-expected-digest')
    reserve = plan['limits']['minFreeDuringBuildGiB'] * 1024**3
    require(shutil.disk_usage(args.task).free >= max(int(size) + reserve,
            plan['limits']['minFreeBeforeBuildGiB'] * 1024**3), 'checkpoint-download-disk')
    try:
        opener.open(urllib.request.Request(base + 'artifacts/' + artifact_id + '/zip', headers=headers), timeout=30)
        raise RuntimeError('checkpoint-expected-redirect')
    except urllib.error.HTTPError as error:
        require(error.code == 302 and bool(error.headers.get('Location')), 'checkpoint-download-redirect')
        location = error.headers['Location']
    require(location.startswith('https://'), 'checkpoint-download-protocol')
    origin = float((args.task / 'started-monotonic').read_text())
    deadline = min(time.monotonic() + 1200, origin + 40 * 60)
    path = args.task / spec['artifactName']
    secure = urllib.request.build_opener(HTTPSRedirect)
    with secure.open(location, timeout=30) as response, path.open('xb') as output:
        count = 0
        while data := response.read(1024**2):
            count += len(data)
            require(count <= int(size) and time.monotonic() < deadline, 'checkpoint-download-bound')
            require(shutil.disk_usage(args.task).free >= reserve + len(data), 'checkpoint-download-disk')
            output.write(data)
    require(count == int(size) and digest(path) == checksum, 'checkpoint-download-integrity')
    with path.open('rb') as stream:
        require(stream.read(2) == b'\x1f\x8b', 'checkpoint-raw-gzip')
    write_json(args.task / 'checkpoint-download.json', {'bytes': count, 'sha256': checksum,
               'manifestSHA256': manifest_hash, 'artifact': int(artifact_id), 'producerJob': producer[0]['id']})
    print(json.dumps({'checkpointDownload': {'artifact': int(artifact_id), 'bytes': count,
                                            'sha256': checksum, 'signedURLPublished': False}}), flush=True)


if __name__ == '__main__':
    main()
