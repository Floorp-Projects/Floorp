# SPDX-License-Identifier: MPL-2.0
"""Encrypt fixed diagnostic snapshots; never publish plaintext or grant qualification."""
import argparse
from contextlib import ExitStack
import hashlib
import io
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tarfile
import tempfile
import time

from firefox_target_observer import _FixedRead, decode, identity
from fresh_job_origin import (DIAGNOSTICS_BRANCH, DIAGNOSTICS_PROFILE, NSS_BRANCH, NSS_PROFILE,
                              NSS_BINDING_BRANCH, NSS_BINDING_PROFILE,
                              VERIFIER_ORDER_BRANCH, VERIFIER_ORDER_PROFILE, REPOSITORY)
from qa_core import Failure, require

CONTRACT = 'fresh45-encrypted-diagnostics-v1'
MIB = 1024**2
FILES = (
    ('job-origin.json', 64*1024), ('summary.json', 16*MIB),
    ('first-failure.json', MIB), ('pipeline.private.log', 8*MIB),
    ('mozilla-upstream-xpcom/receipt.json', 8*MIB),
    ('mozilla-upstream-xpcom/first-failure.json', MIB),
    ('mozilla-upstream-xpcom/native-private/harness.raw.jsonl', 16*MIB),
    ('mozilla-upstream-xpcom/mozilla-xpcshell.private.log', 32*MIB),
)
SCOPED = tuple(name for name, _ in FILES[-2:])
NSS_PROBE_FILES = (
    ('nss-stack-fixer-probe.private.log', MIB),
    ('mozilla-nss-load/native-private/harness.raw.jsonl', MIB),
    ('mozilla-nss-load/mozilla-xpcshell.private.log', MIB),
)
PREFLIGHT_FILES = (
    ('monitor-preflight.json', 2*MIB), ('checkpoint-tests.private.log', 16*MIB),
    ('failure-recording-tests.private.log', 2*MIB), ('monitor-preflight.private.log', 2*MIB),
    ('census-abi-compile.private.log', MIB),
)
ALLOWED_REASONS = ('missing', 'size-limit', 'total-limit', 'file-kind', 'file-binding',
                  'directory-binding', 'root-binding', 'permission', 'io-unavailable')


def _origin_profile(nss_origin, nss_binding_origin, verifier_order_origin, code):
    selectors = (nss_origin, nss_binding_origin, verifier_order_origin)
    require(all(type(value) is bool for value in selectors) and sum(selectors) <= 1, code)
    if verifier_order_origin:
        return VERIFIER_ORDER_BRANCH, VERIFIER_ORDER_PROFILE
    if nss_binding_origin:
        return NSS_BINDING_BRANCH, NSS_BINDING_PROFILE
    return (NSS_BRANCH, NSS_PROFILE) if nss_origin else (DIAGNOSTICS_BRANCH, DIAGNOSTICS_PROFILE)


def bindings(environment, *, nss_origin=False, nss_binding_origin=False, verifier_order_origin=False):
    env = environment
    branch, _ = _origin_profile(nss_origin, nss_binding_origin, verifier_order_origin, 'diagnostic-run-binding')
    require(env.get('GITHUB_REPOSITORY') == REPOSITORY and
            env.get('GITHUB_REF') == 'refs/heads/'+branch and
            env.get('GITHUB_EVENT_NAME') == 'push' and env.get('GITHUB_RUN_ATTEMPT') == '1',
            'diagnostic-run-binding')
    require(type(env.get('GITHUB_RUN_ID')) is str and
            re.fullmatch('[1-9][0-9]{0,19}', env['GITHUB_RUN_ID']) is not None,
            'diagnostic-run-binding')
    for key in ('GITHUB_SHA', 'FLOORP_FIXTURE_SHA', 'RUNTIME_SHA'):
        require(type(env.get(key)) is str and re.fullmatch('[0-9a-f]{40}', env[key]) is not None,
                'diagnostic-run-binding')
    return {'repository': REPOSITORY, 'ref': env['GITHUB_REF'], 'run': int(env['GITHUB_RUN_ID']),
            'attempt': 1, 'head': env['GITHUB_SHA'], 'fixture': env['FLOORP_FIXTURE_SHA'],
            'runtimeSource': env['RUNTIME_SHA']}


def snapshot(task, expected, *, nss_probes=False, nss_origin=False, nss_binding_origin=False,
             verifier_order_origin=False):
    branch, profile = _origin_profile(nss_origin, nss_binding_origin, verifier_order_origin, 'diagnostic-origin-binding')
    require(expected.get('ref') == 'refs/heads/' + branch,
            'diagnostic-origin-binding')
    with ExitStack() as stack:
        reader = _FixedRead(task, stack, time.monotonic()+30)
        require(stat.S_IMODE(os.fstat(reader.root).st_mode) == 0o700, 'diagnostic-private-root')
        raw, _ = reader.read('job-origin.json', 64*1024)
        require(raw is not None, 'diagnostic-origin-binding')
        origin = decode(raw)
        require(type(origin) is dict and
                all(origin.get(key) == expected[key] and type(origin.get(key)) is type(expected[key])
                    for key in ('repository', 'ref', 'run', 'attempt', 'head')) and
                origin.get('profile') == profile and
                type(origin.get('job')) is int and origin['job'] > 0 and
                origin.get('jobName') == 'fresh-firefox45' and
                type(origin.get('platformMinutes')) is int and origin['platformMinutes'] == 360 and
                type(origin.get('workDeadlineMinutes')) is int and origin['workDeadlineMinutes'] == 345 and
                origin.get('clockResetAllowed') is False, 'diagnostic-origin-binding')
        captured, records = {}, []
        files = FILES + (NSS_PROBE_FILES if nss_probes else ()) + (PREFLIGHT_FILES if nss_binding_origin or verifier_order_origin else ())
        for name, cap in files:
            raw, record = reader.read(name, cap)
            if raw is not None:
                captured[name] = raw
            else:
                require(record['reason'] in ALLOWED_REASONS, 'diagnostic-snapshot-unavailable')
            records.append({'name': name, 'status': 'captured' if raw is not None else 'unavailable',
                            'reason': record['reason'], 'bytes': record['bytes'], 'sha256': record['sha256']})
        reader.verify()
    manifest = {'contract': CONTRACT, 'binding': expected, 'job': origin['job'], 'files': records,
                'scopedFilesCaptured': all(name in captured for name in SCOPED),
                'qualification': None, 'productQualification': False}
    if nss_probes:
        manifest['nssProbeFilesCaptured'] = all(name in captured for name, _ in NSS_PROBE_FILES)
    if nss_binding_origin or verifier_order_origin:
        manifest['preflightFilesCaptured'] = all(name in captured for name, _ in PREFLIGHT_FILES)
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode='w', format=tarfile.USTAR_FORMAT) as archive:
        members = {'manifest.json': (json.dumps(manifest, sort_keys=True)+'\n').encode(), **captured}
        for name, raw in members.items():
            entry = tarfile.TarInfo(name)
            entry.size, entry.mode, entry.mtime = len(raw), 0o600, 0
            archive.addfile(entry, io.BytesIO(raw))
    return payload.getvalue(), manifest


def openssl_run(openssl, arguments, *, payload=None):
    require(Path(openssl).is_absolute(), 'diagnostic-openssl')
    try:
        result = subprocess.run([str(openssl), *arguments], input=payload, capture_output=True,
                                timeout=30, env={'PATH': '/usr/bin:/bin', 'LANG': 'C',
                                                 'OPENSSL_CONF': '/dev/null'})
    except (OSError, subprocess.TimeoutExpired):
        raise Failure('diagnostic-encryption') from None
    require(result.returncode == 0, 'diagnostic-encryption')
    return result.stdout


def export(task, certificate, certificate_sha256, output, *, openssl, environment=None,
           nss_probes=False, nss_origin=False, nss_binding_origin=False, verifier_order_origin=False):
    expected = bindings(os.environ if environment is None else environment, nss_origin=nss_origin,
                        nss_binding_origin=nss_binding_origin, verifier_order_origin=verifier_order_origin)
    require(type(certificate_sha256) is str and
            re.fullmatch('[0-9a-f]{64}', certificate_sha256) is not None, 'diagnostic-recipient')
    version = openssl_run(openssl, ['version'])
    require(re.match(rb'OpenSSL [3-9]\.[0-9]+\.', version) is not None, 'diagnostic-openssl')
    with ExitStack() as stack:
        reader = _FixedRead(Path(certificate).parent, stack, time.monotonic()+5)
        cert, _ = reader.read(Path(certificate).name, 16*1024)
        require(cert is not None and hashlib.sha256(cert).hexdigest() == certificate_sha256,
                'diagnostic-recipient')
        reader.verify()
    output = Path(output)
    require(output.is_absolute() and output.parent.resolve() == output.parent and
            not output.exists() and not output.is_symlink(), 'diagnostic-destination')
    payload, manifest = snapshot(task, expected, nss_probes=nss_probes, nss_origin=nss_origin,
                                 nss_binding_origin=nss_binding_origin, verifier_order_origin=verifier_order_origin)
    with tempfile.TemporaryDirectory(prefix='.diagnostic-recipient-', dir=output.parent) as temporary:
        pinned_cert = Path(temporary)/'recipient.pem'
        pinned_cert.write_bytes(cert)
        pinned_cert.chmod(0o600)
        ciphertext = openssl_run(openssl, ['cms', '-encrypt', '-binary', '-aes-256-gcm',
            '-outform', 'DER', '-recip', str(pinned_cert), '-keyopt', 'rsa_padding_mode:oaep',
            '-keyopt', 'rsa_oaep_md:sha256', '-keyopt', 'rsa_mgf1_md:sha256'], payload=payload)
    require(0 < len(ciphertext) <= 66*MIB and b'\x06\x0b*\x86H\x86\xf7\r\x01\t\x10\x01\x17' in ciphertext[:128],
            'diagnostic-ciphertext')
    descriptor = os.open(output, os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, 'w+b') as stream:
        stream.write(ciphertext)
        stream.flush()
        os.fsync(stream.fileno())
        written = identity(os.fstat(stream.fileno()))
        stream.seek(0)
        require(stream.read(len(ciphertext)+1) == ciphertext and
                identity(os.fstat(stream.fileno())) == written == identity(output.lstat()),
                'diagnostic-write')
    return {'contract': CONTRACT, 'status': 'encrypted', 'binding': expected, 'job': manifest['job'],
            'recipientSHA256': certificate_sha256, 'ciphertextBytes': len(ciphertext),
            'ciphertextSHA256': hashlib.sha256(ciphertext).hexdigest(),
            'scopedFilesCaptured': manifest['scopedFilesCaptured'],
            'files': manifest['files'], 'rawContentPublished': False,
            'qualification': None, 'nativeQualification': False, 'productQualification': False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--certificate-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--openssl', type=Path, required=True)
    parser.add_argument('--nss-probes', action='store_true')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--nss-origin-20261102', action='store_true')
    selection.add_argument('--nss-binding-origin-20261103', action='store_true')
    selection.add_argument('--verifier-order-origin-20261104', action='store_true')
    args = parser.parse_args()
    try:
        result = export(args.task, args.certificate, args.certificate_sha256, args.output,
                        openssl=args.openssl, nss_probes=args.nss_probes,
                        nss_origin=args.nss_origin_20261102,
                        nss_binding_origin=args.nss_binding_origin_20261103,
                        verifier_order_origin=args.verifier_order_origin_20261104)
    except BaseException as error:
        code = str(error) if isinstance(error, Failure) and str(error) in (
            'diagnostic-run-binding', 'diagnostic-private-root', 'diagnostic-origin-binding',
            'diagnostic-snapshot-unavailable', 'diagnostic-openssl', 'diagnostic-encryption',
            'diagnostic-recipient', 'diagnostic-destination', 'diagnostic-ciphertext', 'diagnostic-write') else 'diagnostic-unavailable'
        print(json.dumps({'contract': CONTRACT, 'status': 'unavailable', 'code': code,
                          'rawContentPublished': False, 'productQualification': False}), flush=True)
        return 1
    print(json.dumps(result, sort_keys=True), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
