# SPDX-License-Identifier: MPL-2.0
"""Explicit official SDK container replacement with an unchanged plan anchor.

Only a supplied source_path selects the alternate. Download/API/expiry, archive
content, canonical tree and SDK version checks remain the caller's obligations.
"""
import copy
import datetime
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import time
import zlib

from qa_core import Failure, require


DEFAULT_QUALIFICATION = 'plan-sdk-pin-v1'
QUALIFICATION = 'official-artifact-identical-uncompressed-tar-v1'
BASELINE_PLAN_SHA256 = '33fb00c07a7ad9f66950489f06c0b5039ee173bc78297bc13b39265b24f4d460'
BASELINE_TAR_SHA256 = '2002b346bf7087cc14f7b0e52bfc25ed767af29461ae1106356feaaec841dcc4'
RUNTIME_REF = '7dd619742b25567e479eeda97ae071d10542ad0b'
PRODUCER_JOB = 118334374715
PRODUCER_NAME = 'Mac build / download-macos-sdk'
PIN = {'artifact': 12225585543, 'run': 39332628358, 'attempt': 1, 'name': 'macos-sdk', 'entry': 'macos_sdk.tar.gz', 'zipBytes': 105082683, 'zipSHA256': '4d4b7e003737a3f6db74563d1d6aff4d9776fe05d721e26756a801e6b0a0c106', 'tarBytes': 105567985, 'tarSHA256': 'b60f658599f24fea098271b1c4abc5ad3e7b681712bd2269a732432c95f06d38', 'draftReaderTreeSHA256': 'c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f', 'version': '26.5', 'expiresObserved': '2026-10-20T22:08:04Z'}
UNCOMPRESSED_TAR = {'bytes': 760518144,
                    'sha256': '57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3'}
CONTENT_KEYS = ('name', 'entry', 'tarBytes', 'version', 'draftReaderTreeSHA256')
SOURCE_KEYS = frozenset(('schema', 'qualification', 'baselinePlanSHA256', 'baselineTarSHA256',
                         'runtimeRef', 'producerJob', 'pin', 'uncompressedTar'))
CURRENT_PRODUCER_REF = '0ac255739cdd64e100cd6913329247421b0ec2ea'
CURRENT_PRODUCER_JOB = 121368405809
CURRENT_PRODUCER_NAME = 'Mac build [PGO] / download-macos-sdk'
CURRENT_PIN = {**PIN, 'artifact': 12624490543, 'run': 40259426572,
    'zipSHA256': '4090088d6c29b7ca8e6fdcae46c491827c575764d2aea0734d422ef55f9a7bb6',
    'tarSHA256': 'f30c8e392d8c01d595313ec1e53d4d8d8fe62f2e5f8c65638ac4146b52899add',
    'expiresObserved': '2026-10-27T02:31:18Z'}
RECOVERED_PRODUCER_REF = 'a2bceca6e076c130cb133e490e0f7c5f50f34c30'
RECOVERED_PRODUCER_JOB = 122231898010
RECOVERED_PRODUCER_NAME = 'download-macos-sdk'
RECOVERED_PIN = {**PIN, 'artifact': 12739697223, 'run': 40516581212,
    'zipSHA256': '2f8aa854ddae53d2edb6fa300e0583cb823a4ef20c419a810fbf001d414c8c23',
    'tarSHA256': 'cefd8301ce7c40231896cf5ef0c58ef9377d27b9d6a0506067f3b38112a4b8a0',
    'expiresObserved': '2026-10-29T13:57:37Z'}
MAX_UNCOMPRESSED_BYTES = 1024**3
MAX_COMPRESSED_BYTES = 512 * 1024**2
EXACT_QUALIFICATION = 'official-artifact-exact-profile-v2'
EXACT_PROFILE = {
    'profileSHA256': 'ce50e72c5aa2d163a39ba6de283c58f1e2930fb5e261a22ddb6160f54edaaf42',
    'producerRepositoryID': 914860957,
    'producerRepository': 'Floorp-Projects/Floorp-Runtime',
    'producerWorkflow': '.github/workflows/qa-sdk-single-image-20261031.yml',
    'producerContext': {'ImageOS': 'macos26', 'ImageVersion': '20261026.0455.2',
        'machine': 'arm64', 'macOSVersion': '26.6.2', 'pythonVersion': '3.9.6',
        'xcodeVersion': '26.6', 'xcodeBuild': '17F113', 'sdkVersion': '26.5'},
}
EXACT_PRODUCER_REF = '032085d9c0176b68d405fb7d3297629df6c1a971'
EXACT_PRODUCER_JOB = 124313694921
EXACT_PRODUCER_NAME = 'single-image-sdk'
EXACT_PIN = {**PIN, 'artifact': 13011079529, 'run': 41128679337,
    'name': 'macos-sdk-single-image-v4', 'zipBytes': 105157519,
    'zipSHA256': '8d1fc67bc1b130e61c1fd334dcfdadd525a6bf095ad2d50d76a5626950d93aca',
    'tarBytes': 105599920,
    'tarSHA256': '15c6c4ba110a5ff7c231f85a352c5e226f5407243c961d5a04a548d73f3886de',
    'expiresObserved': '2026-11-13T21:08:48Z'}
EXACT_UNCOMPRESSED_TAR = {'bytes': 760518144,
    'sha256': 'f0d4937d3793e5bd0d17ec899df993fb3b68267dab88ddaa9bdc14ccab7c4a58'}


def _same(value, expected):
    if type(value) is not type(expected):
        return False
    if type(expected) is dict:
        return set(value) == set(expected) and all(_same(value[key], item) for key, item in expected.items())
    return value == expected


def _pairs(values):
    result = {}
    for key, value in values:
        require(key not in result, 'sdk-source-json')
        result[key] = value
    return result


def _constant(value):
    raise Failure('sdk-source-json')


def _source_json(path):
    try:
        with Path(path).open('rb') as stream:
            raw = stream.read(16385)
        require(len(raw) <= 16384, 'sdk-source-size')
        value = json.loads(raw.decode('utf-8'), object_pairs_hook=_pairs, parse_constant=_constant)
    except (OSError, ValueError, TypeError, RecursionError):
        raise Failure('sdk-source-read') from None
    return value, hashlib.sha256(raw).hexdigest()


def load_sdk_source(plan_path, source_path=None):
    """Return a trusted spec; default preserves plan.sdk without extra guards."""
    try:
        raw = Path(plan_path).read_bytes()
        plan = json.loads(raw)
        pin, runtime = plan['sdk'], plan['runtime']['ref']
        result = {'pin': copy.deepcopy(pin), 'runtimeRef': runtime, 'producerJob': None, 'producerName': None,
                  'uncompressedTar': None, 'sourceSHA256': None,
                  'baselinePlanSHA256': hashlib.sha256(raw).hexdigest(),
                  'qualification': DEFAULT_QUALIFICATION}
    except (OSError, ValueError, TypeError, KeyError, RecursionError):
        raise Failure('sdk-source-plan-read') from None
    if source_path is None:
        return result
    value, source_sha = _source_json(source_path)
    require(type(value) is dict and type(value.get('schema')) is int and value['schema'] in (1, 2, 3, 4),
            'sdk-source-shape')
    if value['schema'] == 4:
        return _load_exact_source(result, value, source_sha, pin, runtime)
    current = value['schema'] in (2, 3)
    recovered = value['schema'] == 3
    require(set(value) == (SOURCE_KEYS | {'producerRef'} if current else SOURCE_KEYS), 'sdk-source-shape')
    require(_same(value['qualification'], QUALIFICATION), 'sdk-source-qualification')
    require(_same(value['baselinePlanSHA256'], BASELINE_PLAN_SHA256) and
            result['baselinePlanSHA256'] == value['baselinePlanSHA256'], 'sdk-source-plan-binding')
    require(type(pin) is dict and _same(value['baselineTarSHA256'], BASELINE_TAR_SHA256) and
            _same(pin.get('tarSHA256'), value['baselineTarSHA256']), 'sdk-source-baseline-tar')
    require(_same(value['runtimeRef'], RUNTIME_REF) and _same(runtime, RUNTIME_REF), 'sdk-source-runtime')
    producer_job = CURRENT_PRODUCER_JOB if current else PRODUCER_JOB
    producer_name = CURRENT_PRODUCER_NAME if current else PRODUCER_NAME
    expected_pin = CURRENT_PIN if current else PIN
    producer_ref = CURRENT_PRODUCER_REF
    if recovered:
        producer_job, producer_name = RECOVERED_PRODUCER_JOB, RECOVERED_PRODUCER_NAME
        expected_pin, producer_ref = RECOVERED_PIN, RECOVERED_PRODUCER_REF
    require(_same(value['producerJob'], producer_job), 'sdk-source-producer')
    if current:
        require(_same(value['producerRef'], producer_ref), 'sdk-source-producer-ref')
    require(_same(value['pin'], expected_pin), 'sdk-source-pin')
    require(all(_same(value['pin'][key], pin.get(key)) for key in CONTENT_KEYS), 'sdk-source-content-binding')
    require(_same(value['uncompressedTar'], UNCOMPRESSED_TAR), 'sdk-source-uncompressed-anchor')
    result['pin'].update({key: value['pin'][key] for key in expected_pin})
    result.update(producerJob=producer_job, producerName=producer_name, uncompressedTar=dict(UNCOMPRESSED_TAR),
                  sourceSHA256=source_sha, qualification=QUALIFICATION)
    if current:
        result['producerRef'] = producer_ref
    return result


def _load_exact_source(result, value, source_sha, pin, runtime):
    expected = {'schema': 4, 'qualification': EXACT_QUALIFICATION,
        'baselinePlanSHA256': BASELINE_PLAN_SHA256, 'baselineTarSHA256': BASELINE_TAR_SHA256,
        'runtimeRef': RUNTIME_REF, 'producerRef': EXACT_PRODUCER_REF,
        'producerJob': EXACT_PRODUCER_JOB, 'pin': EXACT_PIN,
        'uncompressedTar': EXACT_UNCOMPRESSED_TAR, 'exactProfile': EXACT_PROFILE}
    require(_same(value, expected), 'sdk-source-v4-binding')
    require(result['baselinePlanSHA256'] == BASELINE_PLAN_SHA256 and
            _same(pin.get('tarSHA256'), BASELINE_TAR_SHA256), 'sdk-source-plan-binding')
    require(_same(runtime, RUNTIME_REF), 'sdk-source-runtime')
    require(all(_same(EXACT_PIN[key], pin.get(key)) for key in
                ('entry', 'version', 'draftReaderTreeSHA256')), 'sdk-source-content-binding')
    result.update(pin=copy.deepcopy(EXACT_PIN), producerRef=EXACT_PRODUCER_REF,
        producerJob=EXACT_PRODUCER_JOB, producerName=EXACT_PRODUCER_NAME,
        uncompressedTar=copy.deepcopy(EXACT_UNCOMPRESSED_TAR), sourceSHA256=source_sha,
        qualification=EXACT_QUALIFICATION, exactProfile=copy.deepcopy(EXACT_PROFILE))
    return result


def validate_exact_metadata(spec, artifact, run, producer, now):
    if 'exactProfile' not in spec:
        return
    pin, profile = spec['pin'], spec['exactProfile']
    require(_same(profile, EXACT_PROFILE), 'sdk-exact-profile')
    lineage = {'id': pin['run'], 'repository_id': profile['producerRepositoryID'],
        'head_repository_id': profile['producerRepositoryID'],
        'head_branch': 'b/qa-handoff-20261026', 'head_sha': spec['producerRef']}
    require(_same(artifact.get('id'), pin['artifact']) and
            _same(artifact.get('name'), pin['name']) and
            _same(artifact.get('size_in_bytes'), pin['zipBytes']) and
            _same(artifact.get('digest'), 'sha256:' + pin['zipSHA256']) and
            artifact.get('expired') is False and
            _same(artifact.get('workflow_run'), lineage) and
            _same(artifact.get('expires_at'), pin['expiresObserved']), 'sdk-exact-artifact')
    require(all(_same(run.get(key), expected) for key, expected in {
        'id': pin['run'], 'run_attempt': pin['attempt'], 'head_sha': spec['producerRef'],
        'head_branch': lineage['head_branch'], 'event': 'push',
        'path': profile['producerWorkflow'], 'status': 'completed', 'conclusion': 'success'}.items())
        and _same(run.get('repository', {}).get('id'), profile['producerRepositoryID'])
        and _same(run.get('repository', {}).get('full_name'), profile['producerRepository']),
        'sdk-exact-run')
    require(all(_same(producer.get(key), expected) for key, expected in {
        'id': spec['producerJob'], 'run_id': pin['run'], 'run_attempt': pin['attempt'],
        'head_sha': spec['producerRef'], 'name': spec['producerName'],
        'status': 'completed', 'conclusion': 'success', 'labels': ['macos-26']}.items()),
        'sdk-exact-producer')
    expires = datetime.datetime.fromisoformat(pin['expiresObserved'].replace('Z', '+00:00'))
    require(expires > now, 'sdk-expired')


def _alternate(spec):
    require(type(spec) is dict and 'sourceSHA256' in spec, 'sdk-source-spec')
    source = spec['sourceSHA256']
    require(source is None or (type(source) is str and re.fullmatch('[0-9a-f]{64}', source) is not None),
            'sdk-source-spec')
    return source is not None


def verify_uncompressed_tar(path, spec, *, deadline=None):
    """Hash every decompressed TAR byte within the exact expected byte cap."""
    if not _alternate(spec):
        return None
    def budget():
        require(deadline is None or (type(deadline) in (int, float) and
                math.isfinite(deadline) and time.monotonic() < deadline), 'download-deadline')
    budget()
    anchor = spec.get('uncompressedTar')
    require(type(anchor) is dict and set(anchor) == {'bytes', 'sha256'} and
            type(anchor['bytes']) is int and 0 < anchor['bytes'] <= MAX_UNCOMPRESSED_BYTES and
            type(anchor['sha256']) is str and re.fullmatch('[0-9a-f]{64}', anchor['sha256']) is not None,
            'sdk-source-uncompressed-spec')
    try:
        path = Path(path)
        before = path.lstat()
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and
                0 < before.st_size <= MAX_COMPRESSED_BYTES, 'sdk-source-uncompressed-file')
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(descriptor, 'rb') as compressed:
            opened = os.fstat(compressed.fileno())
            fields = ('st_dev', 'st_ino', 'st_mode', 'st_uid', 'st_size', 'st_mtime_ns', 'st_ctime_ns')
            require(all(getattr(before, key) == getattr(opened, key) for key in fields),
                    'sdk-source-uncompressed-file-changed')
            checksum, count = hashlib.sha256(), 0
            with gzip.GzipFile(fileobj=compressed, mode='rb') as stream:
                while chunk := stream.read(min(1024**2, anchor['bytes'] + 1 - count)):
                    budget()
                    count += len(chunk)
                    require(count <= anchor['bytes'], 'sdk-source-uncompressed-limit')
                    checksum.update(chunk)
            after = os.fstat(compressed.fileno())
            budget()
            current = path.lstat()
            require(all(getattr(before, key) == getattr(after, key) == getattr(current, key) for key in fields),
                    'sdk-source-uncompressed-file-changed')
        require(count == anchor['bytes'], 'sdk-source-uncompressed-bytes')
        require(checksum.hexdigest() == anchor['sha256'], 'sdk-source-uncompressed-sha256')
    except Failure:
        raise
    except (OSError, EOFError, ValueError, TypeError, zlib.error):
        raise Failure('sdk-source-uncompressed-read') from None
    return {'bytes': count, 'sha256': checksum.hexdigest()}


def acquisition_receipt(spec):
    """Build evidence only from a spec loaded from the fixed plan/source."""
    pin = spec['pin']
    result = {key: pin[key] for key in ('artifact', 'run', 'attempt', 'zipSHA256', 'tarSHA256')}
    result.update(bytes=pin['tarBytes'], crcVerified=True, signedURLPublished=False)
    if _alternate(spec):
        result.update(sourceSHA256=spec['sourceSHA256'], baselinePlanSHA256=spec['baselinePlanSHA256'],
                      qualification=spec['qualification'], uncompressedTar=copy.deepcopy(spec['uncompressedTar']))
        if 'producerRef' in spec:
            result['producerRef'] = spec['producerRef']
        if 'exactProfile' in spec:
            result['exactProfile'] = copy.deepcopy(spec['exactProfile'])
    return result


def source_provenance(spec):
    fields = ('qualification', 'sourceSHA256', 'baselinePlanSHA256', 'uncompressedTar', 'producerJob')
    if 'producerRef' in spec:
        fields += ('producerRef',)
    if 'exactProfile' in spec:
        fields += ('exactProfile',)
    return {key: copy.deepcopy(spec[key]) for key in fields}


def validate_acquisition_receipt(value, spec):
    """Reject foreign, partial, extra, or bool-as-int evidence; return trusted data."""
    expected = acquisition_receipt(spec)
    require(_same(value, expected), 'sdk-source-acquisition-receipt')
    return expected
