# SPDX-License-Identifier: MPL-2.0
import argparse
import datetime
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request
import zipfile
from support import Failure, Runner, digest, download, require, write_json


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *_args):
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    pin = plan['sdk']
    token = os.environ['SDK_READ_TOKEN']
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}
    base = 'https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/actions/'
    opener = urllib.request.build_opener(NoRedirect)
    def api(path):
        with opener.open(urllib.request.Request(base + path, headers=headers), timeout=30) as response:
            data = response.read(1024**2 + 1)
            require(len(data) <= 1024**2, 'sdk-metadata-limit')
            return json.loads(data)
    artifact = api('artifacts/' + str(pin['artifact']))
    run = api('runs/' + str(pin['run']))
    require(artifact['id'] == pin['artifact'] and artifact['name'] == pin['name'] and
            artifact['size_in_bytes'] == pin['zipBytes'] and artifact['digest'] == 'sha256:' + pin['zipSHA256'] and
            not artifact['expired'] and artifact['workflow_run']['id'] == pin['run'], 'sdk-artifact-provenance')
    require(run['run_attempt'] == pin['attempt'] and run['head_sha'] == plan['runtime']['ref'] and
            run['status'] == 'completed', 'sdk-run-provenance')
    expires = datetime.datetime.fromisoformat(artifact['expires_at'].replace('Z', '+00:00'))
    now = datetime.datetime.now(datetime.timezone.utc)
    require(expires > now, 'sdk-expired')
    try:
        opener.open(urllib.request.Request(base + 'artifacts/' + str(pin['artifact']) + '/zip', headers=headers), timeout=30)
        raise Failure('sdk-expected-download-redirect')
    except urllib.error.HTTPError as error:
        require(error.code == 302 and bool(error.headers.get('Location')), 'sdk-download-redirect')
        location = error.headers['Location']
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    path = args.output / 'sdk.zip'
    download(location, path, pin['zipSHA256'], pin['zipBytes'])
    with zipfile.ZipFile(path) as archive:
        require(archive.namelist() == [pin['entry']], 'sdk-zip-layout')
        info = archive.getinfo(pin['entry'])
        require(info.file_size == pin['tarBytes'] and not info.flag_bits & 1, 'sdk-zip-entry')
        with archive.open(info) as source, (args.output / pin['entry']).open('xb') as destination:
            count = 0
            while chunk := source.read(1024**2):
                count += len(chunk)
                require(count <= pin['tarBytes'], 'sdk-entry-size-limit')
                destination.write(chunk)
    require(count == pin['tarBytes'] and digest(args.output / pin['entry']) == pin['tarSHA256'], 'sdk-entry-integrity')
    write_json(args.output / 'receipt.json', {'artifact': pin['artifact'], 'run': pin['run'],
               'attempt': pin['attempt'], 'zipSHA256': pin['zipSHA256'], 'tarSHA256': pin['tarSHA256'],
               'bytes': count, 'crcVerified': True, 'signedURLPublished': False})
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
