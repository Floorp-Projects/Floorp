# SPDX-License-Identifier: MPL-2.0
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from support import Failure, HTTPSRedirect, Runner, digest, require, write_json
from sdk_source import load_sdk_source, verify_uncompressed_tar, acquisition_receipt


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *_args):
        return None


def acquisition_budget(deadline):
    require(type(deadline) in (int, float), 'download-deadline')
    remaining = deadline - time.monotonic()
    require(math.isfinite(remaining) and remaining > 0,
            'download-deadline')
    return min(30, remaining)


def download_sdk(url, path, pin, deadline):
    require(urllib.parse.urlsplit(url).scheme == 'https', 'download-protocol')
    require(not path.exists(), 'download-destination-exists')
    opener = urllib.request.build_opener(HTTPSRedirect)
    with opener.open(url, timeout=acquisition_budget(deadline)) as response, path.open('xb') as stream:
        count = 0
        while True:
            acquisition_budget(deadline)
            chunk = response.read(1024**2)
            acquisition_budget(deadline)
            if not chunk:
                break
            count += len(chunk)
            require(count <= pin['zipBytes'], 'download-size-limit')
            stream.write(chunk)
    acquisition_budget(deadline)
    require(count == pin['zipBytes'], 'download-size-mismatch')
    require(digest(path) == pin['zipSHA256'], 'download-hash-mismatch')
    acquisition_budget(deadline)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sdk-source', type=Path)
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    source = load_sdk_source(args.plan, args.sdk_source)
    pin = source['pin']
    token = os.environ['SDK_READ_TOKEN']
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}
    base = 'https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/actions/'
    opener = urllib.request.build_opener(NoRedirect)
    # One acquisition clock includes normal API/redirect and payload validation.
    deadline = time.monotonic() + 600
    def api(path):
        with opener.open(urllib.request.Request(base + path, headers=headers), timeout=acquisition_budget(deadline)) as response:
            data = response.read(1024**2 + 1)
            acquisition_budget(deadline)
            require(len(data) <= 1024**2, 'sdk-metadata-limit')
            return json.loads(data)
    artifact = api('artifacts/' + str(pin['artifact']))
    run = api('runs/' + str(pin['run']))
    require(artifact['id'] == pin['artifact'] and artifact['name'] == pin['name'] and
            artifact['size_in_bytes'] == pin['zipBytes'] and artifact['digest'] == 'sha256:' + pin['zipSHA256'] and
            artifact['expired'] is False and artifact['workflow_run']['id'] == pin['run'], 'sdk-artifact-provenance')
    require(type(run['run_attempt']) is int and run['run_attempt'] == pin['attempt'] and
            run['head_sha'] == source.get('producerRef', source['runtimeRef']) and
            run['status'] == 'completed', 'sdk-run-provenance')
    expires = datetime.datetime.fromisoformat(artifact['expires_at'].replace('Z', '+00:00'))
    now = datetime.datetime.now(datetime.timezone.utc)
    require(expires > now, 'sdk-expired')
    if source['producerJob'] is not None:
        producer = api('jobs/' + str(source['producerJob']))
        require(type(producer.get('id')) is int and producer['id'] == source['producerJob'] and
                type(producer.get('run_id')) is int and producer['run_id'] == pin['run'] and
                type(producer.get('run_attempt')) is int and producer['run_attempt'] == pin['attempt'] and
                producer.get('name') == source['producerName'] and
                producer.get('status') == 'completed' and producer.get('conclusion') == 'success',
                'sdk-producer-provenance')
        require(expires > datetime.datetime.now(datetime.timezone.utc), 'sdk-expired')
    try:
        with opener.open(urllib.request.Request(base + 'artifacts/' + str(pin['artifact']) + '/zip', headers=headers), timeout=acquisition_budget(deadline)):
            raise Failure('sdk-expected-download-redirect')
    except urllib.error.HTTPError as error:
        try:
            require(error.code == 302 and bool(error.headers.get('Location')), 'sdk-download-redirect')
            location = error.headers['Location']
        finally:
            error.close()
    acquisition_budget(deadline)
    require(expires > datetime.datetime.now(datetime.timezone.utc), 'sdk-expired')
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    path = args.output / 'sdk.zip'
    download_sdk(location, path, pin, deadline)
    with zipfile.ZipFile(path) as archive:
        require(archive.namelist() == [pin['entry']], 'sdk-zip-layout')
        info = archive.getinfo(pin['entry'])
        require(info.file_size == pin['tarBytes'] and not info.flag_bits & 1, 'sdk-zip-entry')
        with archive.open(info) as member_source, (args.output / pin['entry']).open('xb') as destination:
            count = 0
            while chunk := member_source.read(1024**2):
                acquisition_budget(deadline)
                count += len(chunk)
                require(count <= pin['tarBytes'], 'sdk-entry-size-limit')
                destination.write(chunk)
    require(count == pin['tarBytes'] and digest(args.output / pin['entry']) == pin['tarSHA256'], 'sdk-entry-integrity')
    acquisition_budget(deadline)
    verify_uncompressed_tar(args.output / pin['entry'], source, deadline=deadline)
    acquisition_budget(deadline)
    write_json(args.output / 'receipt.json', acquisition_receipt(source))
    try:
        acquisition_budget(deadline)
    except Failure:
        try:
            (args.output / 'receipt.json').unlink(missing_ok=True)
        except OSError:
            pass
        raise
    path.unlink()
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        code = (error.args[0] if isinstance(error, Failure) else
                'HTTP' + str(error.code) if isinstance(error, urllib.error.HTTPError) else type(error).__name__)
        task = Path(os.environ['TASK_ROOT'])
        plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        runner = Runner(task, float((task / 'started').read_text()), plan['limits'])
        runner.fail('sdk-read', code)
        print(json.dumps({'stage': 'sdk-read', 'status': 'failed', 'code': code}), file=sys.stderr)
        raise SystemExit(1)
