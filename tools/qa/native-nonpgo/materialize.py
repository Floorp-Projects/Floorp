# SPDX-License-Identifier: MPL-2.0
import argparse
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import urllib.error
from support import Failure, Runner, clean_environment, digest, extract_tar, require, scan_tar, write_json
from pipeline import prepare_tools
from sdk_source import load_sdk_source, verify_uncompressed_tar, validate_acquisition_receipt


class InheritedGroup:
    def run(self, _stage, args, cwd, env, _deadline):
        result = subprocess.run(args, cwd=cwd, env=env, check=False)
        require(result.returncode == 0, 'toolchain-installer-exit')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=('sdk', 'tools'))
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--sdk-source', type=Path)
    args = parser.parse_args()
    plan = json.loads(Path(__file__).with_name('plan.json').read_text())
    require(args.sdk_source is None or args.mode == 'sdk', 'sdk-source-mode')
    if args.mode == 'sdk':
        source = load_sdk_source(Path(__file__).with_name('plan.json'), args.sdk_source)
        pin = source['pin']
        if args.sdk_source is not None:
            validate_acquisition_receipt(json.loads((args.task / 'sdk-download/receipt.json').read_text()), source)
        archive = args.task / 'sdk-download' / pin['entry']
        require(digest(archive) == pin['tarSHA256'], 'sdk-file-hash')
        verify_uncompressed_tar(archive, source)
        receipt = scan_tar(archive, max_bytes=1024**3)
        require(receipt['canonicalSHA256'] == pin['draftReaderTreeSHA256'], 'sdk-tree')
        extract_tar(archive, args.task / 'sdk-extracted', max_bytes=1024**3)
        candidates = list((args.task / 'sdk-extracted').rglob('SDKSettings.plist'))
        require(len(candidates) == 1, 'sdk-root')
        sdk = candidates[0].parent
        require(plistlib.loads(candidates[0].read_bytes())['Version'] == pin['version'], 'sdk-version')
        write_json(args.task / 'sdk-materialized.json', {'path': str(sdk), **receipt})
    else:
        env = clean_environment(args.task)
        env['PATH'] += ':/usr/local/bin'
        prepare_tools(plan, args.task, InheritedGroup(), env)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        code = (str(error) if isinstance(error, Failure) else 'HTTP' + str(error.code)
                if isinstance(error, urllib.error.HTTPError) else type(error).__name__)
        task = Path(sys.argv[sys.argv.index('--task') + 1])
        plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        runner = Runner(task, float((task / 'started').read_text()), plan['limits'])
        runner.fail('materialize', code)
        print(json.dumps({'status': 'failed', 'code': code}))
        raise SystemExit(1)
