#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Trusted-base downloader only; never checkout or execute PR source here.
Call from a default-branch workflow job. Never run this with PR-supplied pins.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request
import artifact_preflight as preflight

API = 'https://api.github.com'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def api_request(path, token):
    if not path.startswith('/repos/') or '\n' in path or '\r' in path:
        raise ValueError('non-repository API request')
    return urllib.request.Request(API + path, headers={
        'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'Floorp-native-debug-consumer',
    })


def api_json(path, token):
    with urllib.request.build_opener(NoRedirect).open(api_request(path, token), timeout=60) as response:
        data = response.read(32 * 1024**2 + 1)
    preflight.require(len(data) <= 32 * 1024**2, 'API metadata budget')
    return json.loads(data)


def download_artifact(identifier, token, output):
    # The credential is sent ONLY to api.github.com, not to signed storage URLs.
    try:
        with urllib.request.build_opener(NoRedirect).open(api_request(
                f'/repos/{preflight.RUNTIME}/actions/artifacts/{identifier}/zip', token), timeout=60):
            raise ValueError('expected GitHub artifact redirect')
    except urllib.error.HTTPError as error:
        if error.code != 302:
            raise
        target = error.headers.get('Location', '')
    url = urllib.parse.urlsplit(target)
    preflight.require(url.scheme == 'https' and url.hostname and not url.username and not url.password,
                      'unsafe artifact download redirect')
    # No credentials and no further redirects on the signed-URL request.
    with urllib.request.build_opener(NoRedirect).open(urllib.request.Request(target), timeout=120) as response:
        with output.open('xb') as stream:
            count = 0
            while chunk := response.read(1024 * 1024):
                count += len(chunk)
                preflight.require(count <= preflight.MAX_BYTES, 'download budget exceeded')
                stream.write(chunk)


def verify_pr_lock(lock, pin):
    preflight.require(lock.get('schemaVersion') == 1 and lock.get('source', {}).get('repository') == preflight.RUNTIME,
                      'foreign production Runtime lock')
    production = lock['source'].get('commit')
    preflight.require(re.fullmatch('[0-9a-f]{40}', production or ''), 'production source is not immutable')
    qualification = pin.get('qualification')
    if production == pin['runtime_sha'] and qualification is None:
        return {'mode': 'matching-runtime', 'production_runtime_sha': production}
    preflight.require(isinstance(qualification, dict) and qualification.get('schema') == 'floorp-debug-qualification-v1'
                      and qualification.get('scope') == 'isolated-browser-xpcshell-qa'
                      and qualification.get('production_runtime_sha') == production
                      and qualification.get('debug_runtime_sha') == pin['runtime_sha']
                      and qualification.get('production_equivalence') is False,
                      'different Debug engine requires an explicit trusted QA qualification record')
    return {'mode': 'separate-debug-qualification', 'production_runtime_sha': production,
            'debug_runtime_sha': pin['runtime_sha'], 'production_equivalence': False}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--pin', type=Path, required=True)
    p.add_argument('--pr', type=int, required=True)
    p.add_argument('--floorp-sha', required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    preflight.require(os.environ.get('GITHUB_REPOSITORY') == preflight.FLOORP
                      and os.environ.get('GITHUB_EVENT_NAME') == 'workflow_dispatch'
                      and os.environ.get('GITHUB_REF') == 'refs/heads/main', 'download requires trusted default-branch dispatch')
    preflight.require(a.pr > 0 and re.fullmatch('[0-9a-f]{40}', a.floorp_sha), 'invalid requested PR/head')
    pin = json.loads(a.pin.read_text())
    preflight.require(pin.get('enabled') is True, 'no approved live Debug artifact pin')
    # Different read tokens: existing Runtime-only credential never queries PR source.
    runtime_token = os.environ['RUNTIME_ARTIFACT_TOKEN']
    floorp_token = os.environ['GITHUB_TOKEN']
    artifact = api_json(f"/repos/{preflight.RUNTIME}/actions/artifacts/{int(pin['artifact_id'])}", runtime_token)
    run = api_json(f"/repos/{preflight.RUNTIME}/actions/runs/{int(pin['run_id'])}/attempts/{int(pin['run_attempt'])}", runtime_token)
    pr = api_json(f'/repos/{preflight.FLOORP}/pulls/{a.pr}', floorp_token)
    context = preflight.validate_metadata(pin, artifact, run, pr, a.floorp_sha)
    lock_result = api_json(f'/repos/{preflight.FLOORP}/contents/floorp-runtime.lock.json?ref={a.floorp_sha}', floorp_token)
    preflight.require(lock_result.get('encoding') == 'base64', 'unsupported PR lock encoding')
    verify_pr_lock(json.loads(base64.b64decode(lock_result['content'], validate=False)), pin)
    a.output.mkdir(parents=True, exist_ok=False)
    download_artifact(pin['artifact_id'], runtime_token, a.output / 'artifact.zip')
    preflight.require(preflight.digest(a.output / 'artifact.zip') == pin['artifact_sha256'], 'downloaded artifact digest mismatch')
    # These are API observations, not native execution results. No signed URL saved.
    for name, value in [('pin',pin),('artifact',artifact),('run',run),('pr',pr),('context',context)]:
        (a.output / (name + '.json')).write_text(json.dumps(value, indent=2) + '\n')

if __name__ == '__main__':
    main()
