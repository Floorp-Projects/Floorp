# SPDX-License-Identifier: MPL-2.0
"""Private APFS boundary controls. Diagnostic completion never qualifies a checkpoint.

No SDK/build/artifact, force detach, privilege change or unknown-process signal.
The writable backing file is deliberately *not* modified by the raw-open control.
The CLI is the sole native APFS integration test; unit tests never create images.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import select
import selectors
import stat
import subprocess
import sys
import tempfile
import time


MAX_OUTPUT = 1024 * 1024
MAX_STORED = 128 * 1024 * 1024
COMMAND_SECONDS = 15
TOTAL_SECONDS = 120
CHILD_OUTPUT = 16384
HDIUTIL = '/usr/bin/hdiutil'
DISKUTIL = '/usr/sbin/diskutil'


class ExperimentFailure(Exception):
    pass


def require(condition, code):
    if not condition:
        raise ExperimentFailure(code)


def error_fact(error):
    """Exception text, command arguments and utility output remain private."""
    return {'kind': type(error).__name__,
            'errno': error.errno if isinstance(error, OSError) else None}


def reap(process, *, natural=False):
    result = {'naturalRelease': natural, 'exitCode': None, 'leaderReaped': False,
              'groupGone': None, 'groupScope': 'inherited-outer-monitored-stage',
              'signalsSent': [], 'failure': None}
    try:
        if process.stdin and not process.stdin.closed:
            process.stdin.close()
        if not natural and process.poll() is None:
            process.terminate()
            result['signalsSent'].append('TERM')
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            if process.poll() is None:
                process.terminate()
                result['signalsSent'].append('TERM')
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                if process.poll() is None:
                    process.kill()
                    result['signalsSent'].append('KILL')
                process.wait(timeout=2)
        result.update(exitCode=process.returncode, leaderReaped=process.returncode is not None)
    except BaseException as error:
        result['failure'] = error_fact(error)
    finally:
        for stream in (process.stdin, process.stdout, process.stderr):
            if stream and not stream.closed:
                try:
                    stream.close()
                except BaseException as error:
                    if result['failure'] is None:
                        result['failure'] = error_fact(error)
    return result


class Commands:
    def __init__(self, result, *, clock=time.monotonic):
        self.result, self.clock = result, clock
        self.deadline = clock() + TOTAL_SECONDS
        self.cleanup_deadline = None
        self.output_bytes = 0

    def remaining(self):
        require(self.clock() < self.deadline, 'experiment-deadline')
        return self.deadline - self.clock()

    def run(self, label, args, *, cleanup=False):
        require(args[0] in (HDIUTIL, DISKUTIL), 'utility-not-allowed')
        require('-force' not in args and '-encryption' not in args and
                not any(value in args for value in ('sudo', '-stdinpass', '-passphrase')),
                'utility-unsafe-option')
        if cleanup:
            if self.cleanup_deadline is None:
                self.cleanup_deadline = self.clock() + 30
            duration = min(COMMAND_SECONDS, self.cleanup_deadline-self.clock())
            require(duration > 0, 'cleanup-deadline')
        else:
            duration = min(COMMAND_SECONDS, self.remaining())
        record = {'stage': label, 'exitCode': None, 'outputBytes': 0,
                  'busyTextSeen': False, 'busyErrnoEstablished': False,
                  'failure': None, 'cleanup': None}
        self.result['commands'].append(record)
        process = None
        started = self.clock()
        output, errors = bytearray(), bytearray()
        try:
            process = subprocess.Popen(args, stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                env={**os.environ, 'LC_ALL': 'C'})
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ, output)
                selector.register(process.stderr, selectors.EVENT_READ, errors)
                while selector.get_map():
                    left = duration - (self.clock() - started)
                    require(left > 0, 'utility-deadline')
                    for key, _ in selector.select(min(left, .1)):
                        chunk = os.read(key.fd, 8192)
                        if not chunk:
                            selector.unregister(key.fileobj)
                            continue
                        require(self.output_bytes + len(chunk) <= MAX_OUTPUT, 'utility-output-limit')
                        self.output_bytes += len(chunk)
                        key.data.extend(chunk)
            process.wait(timeout=max(.01, duration - (self.clock() - started)))
            record['exitCode'] = process.returncode
            record['busyTextSeen'] = bool(re.search(rb'\bbusy\b', errors, re.I))
            return process.returncode, bytes(output)
        except BaseException as error:
            record['failure'] = error_fact(error)
            raise
        finally:
            primary_pending = sys.exc_info()[0] is not None
            record.update(outputBytes=len(output)+len(errors),
                          outputSHA256=hashlib.sha256(output+errors).hexdigest(),
                          elapsedSeconds=round(self.clock()-started, 6))
            if process is not None:
                record['cleanup'] = reap(process)
                clean = record['cleanup']['leaderReaped'] and record['cleanup']['failure'] is None
                if not clean and not primary_pending:
                    raise ExperimentFailure('utility-cleanup-failed')

    def plist(self, label, args, *, cleanup=False):
        code, output = self.run(label, args, cleanup=cleanup)
        require(code == 0, 'utility-nonzero')
        value = plistlib.loads(output)
        require(isinstance(value, dict), 'utility-plist-schema')
        return value


def backing_identity(path):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == os.geteuid() and info.st_nlink == 1,
            'backing-file-identity')
    require(info.st_blocks * 512 <= MAX_STORED, 'backing-storage-limit')
    return info.st_dev, info.st_ino, info.st_uid


def image_entry(info, image):
    image = image.resolve()
    require(isinstance(info.get('images'), list), 'image-info-schema')
    matches = []
    for item in info['images']:
        require(isinstance(item, dict), 'image-info-entry')
        name = item.get('image-path')
        if isinstance(name, str) and Path(name).resolve() == image:
            matches.append(item)
    require(len(matches) <= 1, 'image-info-ambiguous')
    return matches[0] if matches else None


def mount_binding(entry, mount, disk):
    entities = entry.get('system-entities')
    require(isinstance(entities, list) and entities, 'image-entities-schema')
    devices, mounted = [], []
    for entity in entities:
        require(isinstance(entity, dict), 'image-entity-schema')
        device = entity.get('dev-entry')
        if device is not None:
            require(isinstance(device, str) and re.fullmatch(r'/dev/disk[0-9]+(?:s[0-9]+)*', device),
                    'image-device-schema')
            devices.append(device)
        point = entity.get('mount-point')
        if point:
            require(isinstance(point, str) and Path(point).resolve() == mount,
                    'image-other-mount-refused')
            mounted.append(device)
    require(len(mounted) == 1 and len(devices) == len(set(devices)), 'image-mount-ambiguous')
    roots = [name for name in devices if re.fullmatch(r'/dev/disk[0-9]+', name)]
    require(roots, 'image-root-device-unavailable')
    require(disk.get('MountPoint') == str(mount) and disk.get('DeviceNode') == mounted[0]
            and disk.get('FilesystemType', '').lower() == 'apfs', 'disk-mount-binding')
    volume = disk.get('VolumeUUID')
    require(isinstance(volume, str) and re.fullmatch(r'[0-9A-Fa-f-]{36}', volume), 'volume-uuid-unavailable')
    require(type(disk.get('ReadOnlyVolume')) is bool and type(disk.get('ReadOnlyMedia')) is bool,
            'disk-readonly-unavailable')
    return {'devices': tuple(devices), 'detach': roots[0], 'uuid': volume.lower(),
            'diskReadOnlyVolume': disk['ReadOnlyVolume'], 'diskReadOnlyMedia': disk['ReadOnlyMedia']}


class Image:
    def __init__(self, directory, commands, result):
        directory = directory.resolve(strict=True)
        self.directory = directory
        self.image = directory/'private.sparseimage'
        self.mount = directory/'mount'
        self.mount.mkdir(mode=0o700)
        self.commands, self.result = commands, result
        self.backing = self.volume = self.bound = None

    def create(self):
        code, _ = self.commands.run('image-create', [HDIUTIL, 'create', '-size', '256m',
            '-fs', 'APFS', '-type', 'SPARSE', '-volname', self.directory.name, str(self.image)])
        require(code == 0, 'image-create-failed')
        self.backing = backing_identity(self.image)
        self.result['imageCreated'] = True

    def entry(self, *, cleanup=False):
        require(self.backing is not None and backing_identity(self.image) == self.backing,
                'backing-identity-changed')
        info = self.commands.plist('image-info', [HDIUTIL, 'info', '-plist'], cleanup=cleanup)
        require(backing_identity(self.image) == self.backing, 'backing-identity-changed')
        return image_entry(info, self.image)

    def verify(self, *, readonly=None, cleanup=False):
        require(not self.mount.is_symlink() and self.mount.resolve() == self.mount,
                'mount-path-changed')
        entry = self.entry(cleanup=cleanup)
        require(entry is not None, 'owned-image-not-attached')
        disk = self.commands.plist('disk-info', [DISKUTIL, 'info', '-plist', str(self.mount)],
                                   cleanup=cleanup)
        value = mount_binding(entry, self.mount, disk)
        require(self.volume is None or self.volume == value['uuid'], 'volume-uuid-changed')
        require(self.bound is None or self.bound['devices'] == value['devices'], 'image-devices-changed')
        self.volume = value['uuid']
        flags = os.statvfs(self.mount).f_flag
        is_ro = bool(flags & os.ST_RDONLY)
        if readonly is not None:
            require(is_ro is readonly and value['diskReadOnlyVolume'] is readonly,
                    'readonly-state-mismatch')
            if readonly:
                require(value['diskReadOnlyMedia'], 'readonly-media-not-established')
        self.bound = value
        self.result['bindings'].append({'backingIdentityMatches': True, 'imagePathMatches': True,
            'systemEntitiesMatch': True, 'canonicalMountMatches': True, 'volumeUUIDConsistent': True,
            'statvfsReadOnly': is_ro, 'diskReadOnlyVolume': value['diskReadOnlyVolume'],
            'diskReadOnlyMedia': value['diskReadOnlyMedia'],
            'backingStoredBytes': self.image.stat().st_blocks*512})
        return value

    def attach(self, readonly=False):
        require(self.entry() is None, 'existing-image-attachment-refused')
        require(not os.path.ismount(self.mount) and not any(self.mount.iterdir()),
                'mount-target-not-empty')
        args = [HDIUTIL, 'attach', '-plist', '-nobrowse', '-mountpoint', str(self.mount)]
        if readonly:
            args.append('-readonly')
        args.append(str(self.image))
        attached = self.commands.plist('image-attach-ro' if readonly else 'image-attach-rw', args)
        require(isinstance(attached.get('system-entities'), list), 'attach-plist-schema')
        self.bound = None
        value = self.verify(readonly=readonly)
        require(set(e.get('dev-entry') for e in attached['system-entities'] if isinstance(e, dict)
                    and e.get('dev-entry')) == set(value['devices']), 'attach-info-devices-mismatch')

    def detach(self, *, held=False, cleanup=False):
        before = self.verify(cleanup=cleanup)
        code, _ = self.commands.run('image-detach-held' if held else 'image-detach',
            [HDIUTIL, 'detach', before['detach']], cleanup=cleanup)
        entry = self.entry(cleanup=cleanup)
        if code == 0:
            require(entry is None and not os.path.ismount(self.mount), 'detach-success-not-confirmed')
            self.bound = None
            return {'outcome': 'detached', 'originalMountContinues': False,
                    'busyErrnoEstablished': False, 'oldHandleWriteAfterDetach': None}
        require(held, 'normal-detach-failed')
        self.verify(readonly=False, cleanup=cleanup)
        return {'outcome': 'refused', 'originalMountContinues': True,
                'busyTextSeen': next(row['busyTextSeen'] for row in reversed(self.result['commands'])
                         if row['stage'] == 'image-detach-held'),
                'busyErrnoEstablished': False, 'oldHandleWriteAfterDetach': None}

    def cleanup(self):
        value = {'normalDetach': None, 'failure': None, 'privateDirectoryRetained': True,
                 'attachmentState': 'unknown', 'cleanupVerified': False}
        try:
            require(self.backing is not None, 'cleanup-backing-not-bound')
            entry = self.entry(cleanup=True)
            if entry is not None:
                self.detach(cleanup=True)
            value.update(normalDetach='verified-not-attached',
                         attachmentState='verified-not-attached', cleanupVerified=True)
        except BaseException as error:
            value['failure'] = {'code': 'owned-image-cleanup-unverified', **error_fact(error)}
        return value


CHILD = r'''
import errno,json,mmap,os,sys
kind,path=sys.argv[1:]
fd=area=None
if kind in ('fd','mapping'):
    fd=os.open(path,os.O_RDWR)
    if kind=='mapping':
        area=mmap.mmap(fd,4096,flags=mmap.MAP_SHARED,prot=mmap.PROT_READ|mmap.PROT_WRITE)
        os.close(fd);fd=None
print(json.dumps({'ready':True,'sameUID':os.geteuid()==int(os.environ['BOUNDARY_EXPECTED_UID']),
                  'fdClosed':kind=='mapping','targetOpened':kind!='late'}),flush=True)
for line in sys.stdin:
    if kind in ('fd','mapping'):
        if area is not None:area[0:1]=b'M';area.flush()
        else:os.pwrite(fd,b'F',0);os.fsync(fd)
        result={'writeCompleted':True}
    else:
        result={}
        def attempt(name,operation):
            try:operation();result[name]={'denied':False,'errno':None}
            except OSError as error:result[name]={'denied':True,'errno':error.errno}
        def opening(flags,target=path):
            value=os.open(target,flags,0o600);os.close(value)
        attempt('readWriteOpen',lambda:opening(os.O_RDWR))
        attempt('create',lambda:opening(os.O_WRONLY|os.O_CREAT|os.O_EXCL,path+'.new'))
        attempt('rename',lambda:os.rename(path,path+'.renamed'))
        readfd=os.open(path,os.O_RDONLY)
        def shared():
            value=mmap.mmap(readfd,4096,flags=mmap.MAP_SHARED,prot=mmap.PROT_READ|mmap.PROT_WRITE);value.close()
        attempt('sharedWriteMapFromReadOnlyFD',shared)
        before=os.pread(readfd,4096,0)
        value=mmap.mmap(readfd,4096,flags=mmap.MAP_PRIVATE,prot=mmap.PROT_READ|mmap.PROT_WRITE)
        value[0:1]=b'P';value.flush()
        result['privateMapping']={'memoryChanged':value[0:1]==b'P','fileUnchanged':os.pread(readfd,4096,0)==before}
        value.close();os.close(readfd)
    print(json.dumps(result),flush=True)
if area is not None:area.close()
if fd is not None:os.close(fd)
'''


class Child:
    def __init__(self, kind, path, commands):
        self.commands, self.buffer, self.bytes = commands, bytearray(), 0
        self.process = subprocess.Popen([sys.executable, '-c', CHILD, kind, str(path)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            env={**os.environ, 'BOUNDARY_EXPECTED_UID': str(os.geteuid())})

    def read(self):
        deadline = time.monotonic() + min(5, self.commands.remaining())
        while b'\n' not in self.buffer:
            remaining = deadline-time.monotonic()
            require(remaining > 0, 'child-response-deadline')
            if not select.select([self.process.stdout], [], [], remaining)[0]:
                raise ExperimentFailure('child-response-deadline')
            chunk = os.read(self.process.stdout.fileno(), 4096)
            require(chunk, 'child-response-eof')
            self.bytes += len(chunk)
            require(self.bytes <= CHILD_OUTPUT, 'child-output-limit')
            self.buffer.extend(chunk)
        line, _, rest = self.buffer.partition(b'\n')
        self.buffer = bytearray(rest)
        value = json.loads(line)
        require(isinstance(value, dict), 'child-response-schema')
        return value

    def command(self):
        self.process.stdin.write(b'run\n')
        self.process.stdin.flush()
        return self.read()


def baseline_for(child):
    if not os.environ.get('FLOORP_CENSUS_ABI'):
        return None, {'available': False, 'childIncludedInBaseline': None,
                      'childNewCandidate': None, 'reason': 'census-abi-not-provided'}
    from checkpoint import owned_processes
    rows = owned_processes()
    matching = [row for row in rows if row[0] == child.process.pid]
    require(len(matching) == 1 and matching[0][3:] == (os.geteuid(), os.getuid()),
            'baseline-child-not-bound')
    return (rows, matching[0]), {'available': True, 'baselineCandidateIdentities': len(rows),
        'childIncludedInBaseline': True, 'childNewCandidate': False, 'baselineReset': False}


def verify_baseline(child, baseline, receipt):
    if baseline is not None:
        from checkpoint import owned_processes
        rows = owned_processes()
        original, row = baseline
        require(row in rows and child.process.poll() is None, 'baseline-child-generation-changed')
        receipt.update(childStillSameBaselineIdentity=True, childNewCandidate=row in rows-original)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def writable_control(kind, image, commands, result, children):
    value = {'kind': kind, 'status': 'collecting', 'census': None, 'cleanup': None}
    result['writerControls'].append(value)
    payload = image.mount/'payload'
    before = sha(payload)
    child = Child(kind, payload, commands)
    children.append((child, value))
    ready = child.read()
    require(ready.get('ready') is True and ready.get('sameUID') is True and
            ready.get('targetOpened') is True and ready.get('fdClosed') is (kind == 'mapping'),
            'writer-child-not-ready')
    baseline, value['census'] = baseline_for(child)
    require(child.command() == {'writeCompleted': True}, 'writer-child-write-failed')
    after = sha(payload)
    require(before != after, 'writer-target-not-changed')
    verify_baseline(child, baseline, value['census'])
    value.update(targetChanged=True, mappingHeldWithFDClosed=kind == 'mapping',
        newCandidateAbsenceDoesNotProveNoWriter=True if baseline is not None else None)
    value['detach'] = image.detach(held=True)
    value['cleanup'] = reap(child.process, natural=True)
    require(value['cleanup']['exitCode'] == 0 and value['cleanup']['leaderReaped'] and
            not value['cleanup']['signalsSent'] and
            value['cleanup']['failure'] is None, 'writer-natural-cleanup-failed')
    if value['detach']['outcome'] == 'detached':
        image.attach()
    image.verify(readonly=False)
    require(sha(payload) == after, 'writer-payload-changed-after-detach')
    value.update(status='observed', payloadHash=after)


def readonly_control(image, commands, result, children):
    payload = image.mount/'payload'
    expected = sha(payload)
    manifest_bytes = json.dumps({'schema': 1, 'payloadSHA256': expected,
        'allPayloadAndRefsFreezeEstablished': False}, sort_keys=True).encode()+b'\n'
    manifest = image.mount/'manifest.json'
    manifest.write_bytes(manifest_bytes)
    manifest_hash = sha(manifest)
    image.detach()
    image.attach(readonly=True)
    require(sha(payload) == expected and sha(manifest) == manifest_hash,
            'readonly-readback-mismatch')
    value = result['readOnlyControl'] = {'status': 'collecting', 'cleanup': None}
    child = Child('late', payload, commands)
    children.append((child, value))
    ready = child.read()
    require(ready.get('ready') is True and ready.get('sameUID') is True and
            ready.get('targetOpened') is False, 'late-child-not-ready')
    baseline, value['census'] = baseline_for(child)
    operations = child.command()
    # Fixed projection: unexpected child text/keys cannot enter public receipts.
    value['operations'] = {key: {'denied': operations.get(key, {}).get('denied') is True,
        'errno': operations.get(key, {}).get('errno') if type(operations.get(key, {}).get('errno')) is int else None}
        for key in ('readWriteOpen', 'create', 'rename', 'sharedWriteMapFromReadOnlyFD')}
    private = operations.get('privateMapping', {})
    value['privateMapping'] = {key: private.get(key) is True for key in ('memoryChanged', 'fileUnchanged')}
    verify_baseline(child, baseline, value['census'])
    value['cleanup'] = reap(child.process, natural=True)
    require(value['cleanup']['exitCode'] == 0 and value['cleanup']['leaderReaped'] and
            not value['cleanup']['signalsSent'] and value['cleanup']['failure'] is None,
            'readonly-child-natural-cleanup-failed')
    for key in ('readWriteOpen', 'create', 'rename'):
        require(value['operations'][key] == {'denied': True, 'errno': errno.EROFS},
                'readonly-write-not-erofs')
    shared = value['operations']['sharedWriteMapFromReadOnlyFD']
    require(shared['denied'] and shared['errno'] in (errno.EACCES, errno.EROFS),
            'readonly-shared-map-not-denied')
    require(all(value['privateMapping'].values()), 'private-map-changed-file')
    image.verify(readonly=True)
    require(sha(payload) == expected and sha(manifest) == manifest_hash,
            'readonly-post-control-hash-mismatch')
    value.update(status='observed', payloadHash=expected, manifestHash=manifest_hash,
                 beforeAfterReadOnlyBindingVerified=True, payloadAndManifestHashesMatch=True)
    raw = result['backingOpenControl'] = {'openedReadWrite': None, 'errno': None,
        'bytesWritten': 0, 'reattachReadWriteAttempted': False, 'sameUIDImmutabilityEstablished': False}
    image.verify(readonly=True)
    descriptor = None
    try:
        descriptor = os.open(image.image, os.O_RDWR | os.O_NOFOLLOW)
        info = os.fstat(descriptor)
        require((info.st_dev, info.st_ino, info.st_uid) == image.backing, 'raw-open-binding-changed')
        raw['openedReadWrite'] = True
    except OSError as error:
        raw.update(openedReadWrite=False, errno=error.errno)
    finally:
        if descriptor is not None:
            os.close(descriptor)
    image.verify(readonly=True)
    require(sha(payload) == expected and sha(manifest) == manifest_hash,
            'readonly-after-raw-open-hash-mismatch')


def run_experiment(task, result):
    result.update(schema=1, status='failed', diagnosticProcedure='incomplete',
        qualification='NO_GO', productionGateReplacement=False,
        checkpointEligibilityEstablished=False, sameUIDImmutabilityEstablished=False,
        allPayloadAndRefsSingleFreezeRequired=True, sdkUsed=False, floorpBuildPerformed=False,
        newArtifactsStored=0, productionGateChanged=False, imageCreated=False,
        commands=[], bindings=[], writerControls=[], readOnlyControl=None,
        backingOpenControl=None, firstFailure=None, childCleanup=[], imageCleanup=None,
        rawProcessIdentitiesPublished=False, rawUtilityOutputPublished=False,
        privateTaskPathsPublished=False, heldDetachProvesSafeFreeze=False,
        processGroupScope='inherited-outer-monitored-stage', stageGroupGone=None,
        stageGroupCleanupAuthority='outer-runner')
    result['qualificationLimits'] = [
        'same-UID backing mutation is not excluded',
        'all checkpoint payload and references are not atomically frozen',
        'reopen or remount capability is not excluded',
        'the production checkpoint route is not exercised']
    commands, image, children = Commands(result), None, []
    started = time.monotonic()
    stage = 'platform'
    try:
        require(sys.platform == 'darwin', 'readonly-experiment-platform')
        stage = 'private-task'
        task = Path(task)
        require(task.is_dir() and not task.is_symlink() and task.stat().st_uid == os.geteuid(),
                'task-directory-not-owned')
        directory = Path(tempfile.mkdtemp(prefix='readonly-boundary-', dir=task.resolve()))
        directory.chmod(0o700)
        image = Image(directory, commands, result)
        stage = 'create-attach'
        image.create()
        image.attach()
        (image.mount/'payload').write_bytes(b'0'*4096)
        for kind in ('fd', 'mapping'):
            stage = 'writable-'+kind
            writable_control(kind, image, commands, result, children)
        stage = 'readonly-controls'
        readonly_control(image, commands, result, children)
        commands.remaining()
        result.update(status='completed', diagnosticProcedure='completed')
    except BaseException as error:
        result['firstFailure'] = {'stage': stage, **error_fact(error),
            'code': str(error) if isinstance(error, ExperimentFailure) else 'diagnostic-exception'}
    finally:
        for child, control in children:
            if control['cleanup'] is None:
                control['cleanup'] = reap(child.process)
            value = control['cleanup']
            result['childCleanup'].append(value)
            if value['failure'] or not value['leaderReaped']:
                result['status'] = 'failed'
                if result['firstFailure'] is None:
                    result['firstFailure'] = {'stage': 'child-cleanup', 'code': 'child-cleanup-unverified'}
        if image is not None:
            result['imageCleanup'] = image.cleanup()
            if result['imageCleanup']['failure']:
                result['status'] = 'failed'
                if result['firstFailure'] is None:
                    result['firstFailure'] = {'stage': 'image-cleanup', 'code': 'image-cleanup-unverified'}
        result.update(elapsedSeconds=round(time.monotonic()-started, 6),
                      utilityOutputBytes=commands.output_bytes)
        if result['status'] != 'completed':
            result['diagnosticProcedure'] = 'failed'
        # All outcomes, including a successful RO filesystem experiment, stay NO_GO.
        result.update(qualification='NO_GO', productionGateReplacement=False,
            checkpointEligibilityEstablished=False, sameUIDImmutabilityEstablished=False)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    args = parser.parse_args()
    result = {}
    try:
        run_experiment(args.task, result)
    finally:
        try:
            path = args.task/'readonly-boundary-experiment.json'
            descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(descriptor, 'w') as stream:
                json.dump(result, stream, sort_keys=True)
                stream.write('\n')
        except BaseException as error:
            result.update(status='failed', evidenceWriteFailure=error_fact(error))
        print(json.dumps({'readonlyBoundaryExperiment': result}), flush=True)
    return 0 if result.get('status') == 'completed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
