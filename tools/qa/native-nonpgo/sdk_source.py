# SPDX-License-Identifier: MPL-2.0
"""Explicit official SDK container replacement with an unchanged plan anchor.

Only a supplied source_path selects the alternate. Download/API/expiry, archive
content, canonical tree and SDK version checks remain the caller's obligations.
"""
import copy
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
MAX_UNCOMPRESSED_BYTES = 1024**3
MAX_COMPRESSED_BYTES = 512 * 1024**2


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
    current = type(value) is dict and type(value.get('schema')) is int and value['schema'] == 2
    require(type(value) is dict and set(value) == (SOURCE_KEYS | {'producerRef'} if current else SOURCE_KEYS) and
            type(value.get('schema')) is int and value['schema'] == (2 if current else 1), 'sdk-source-shape')
    require(_same(value['qualification'], QUALIFICATION), 'sdk-source-qualification')
    require(_same(value['baselinePlanSHA256'], BASELINE_PLAN_SHA256) and
            result['baselinePlanSHA256'] == value['baselinePlanSHA256'], 'sdk-source-plan-binding')
    require(type(pin) is dict and _same(value['baselineTarSHA256'], BASELINE_TAR_SHA256) and
            _same(pin.get('tarSHA256'), value['baselineTarSHA256']), 'sdk-source-baseline-tar')
    require(_same(value['runtimeRef'], RUNTIME_REF) and _same(runtime, RUNTIME_REF), 'sdk-source-runtime')
    producer_job = CURRENT_PRODUCER_JOB if current else PRODUCER_JOB
    producer_name = CURRENT_PRODUCER_NAME if current else PRODUCER_NAME
    expected_pin = CURRENT_PIN if current else PIN
    require(_same(value['producerJob'], producer_job), 'sdk-source-producer')
    if current:
        require(_same(value['producerRef'], CURRENT_PRODUCER_REF), 'sdk-source-producer-ref')
    require(_same(value['pin'], expected_pin), 'sdk-source-pin')
    require(all(_same(value['pin'][key], pin.get(key)) for key in CONTENT_KEYS), 'sdk-source-content-binding')
    require(_same(value['uncompressedTar'], UNCOMPRESSED_TAR), 'sdk-source-uncompressed-anchor')
    result['pin'].update({key: value['pin'][key] for key in expected_pin})
    result.update(producerJob=producer_job, producerName=producer_name, uncompressedTar=dict(UNCOMPRESSED_TAR),
                  sourceSHA256=source_sha, qualification=QUALIFICATION)
    if current:
        result['producerRef'] = CURRENT_PRODUCER_REF
    return result


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
    return result


def source_provenance(spec):
    fields = ('qualification', 'sourceSHA256', 'baselinePlanSHA256', 'uncompressedTar', 'producerJob')
    if 'producerRef' in spec:
        fields += ('producerRef',)
    return {key: copy.deepcopy(spec[key]) for key in fields}


def validate_acquisition_receipt(value, spec):
    """Reject foreign, partial, extra, or bool-as-int evidence; return trusted data."""
    expected = acquisition_receipt(spec)
    require(_same(value, expected), 'sdk-source-acquisition-receipt')
    return expected
