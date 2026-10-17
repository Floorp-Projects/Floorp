# SPDX-License-Identifier: MPL-2.0
import copy
import errno
import io
import json
import os
from pathlib import Path
import plistlib
import struct
import sys
import tarfile
import tempfile
import time
import unittest
from unittest import mock
from support import Failure, Runner, clean_environment, extract_tar, scan_tar
from native_suite import context_result, failure_snapshot
from pipeline import macho, verify_config
from owned_native import OwnedNative


class Guards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_effective_config_uses_upstream_host_cpu_arch(self):
        tools, sdk = self.root / 'tools', self.root / 'sdk'
        substs = {'TARGET_CPU': 'x86_64', 'HOST_CPU_ARCH': 'x86_64', 'OS_ARCH': 'Darwin',
                  'OS_TARGET': 'Darwin', 'CC_TYPE': 'clang', 'CC_VERSION': '22.1.8',
                  'MOZ_APP_VERSION': '158.0.1', 'MOZ_DEBUG': '1', 'ENABLE_TESTS': '1',
                  'MOZ_SANDBOX': '1', 'MACOSX_DEPLOYMENT_TARGET': '10.15',
                  'MOZ_APP_DISPLAYNAME': 'Floorp', 'MOZ_CHILD_PROCESS_NAME': 'plugin-container',
                  'MOZ_CHILD_PROCESS_BUNDLENAME': 'plugin-container.app'}
        for key, path in (('CC', 'clang/bin/clang'), ('CXX', 'clang/bin/clang++'),
                          ('HOST_CC', 'clang/bin/clang'), ('HOST_CXX', 'clang/bin/clang++'),
                          ('RUSTC', 'rust/bin/rustc'), ('CARGO', 'rust/bin/cargo'),
                          ('NODEJS', 'node/bin/node'), ('WASI_SYSROOT', 'sysroot-wasm32-wasi'),
                          ('ONNX_RUNTIME', 'onnxruntime-x86_64-apple-darwin/libonnxruntime.dylib')):
            substs[key] = str(tools / path)
        substs['MACOS_SDK_DIR'] = str(sdk)
        (self.root / 'obj').mkdir()
        (self.root / 'obj/mozinfo.json').write_text(json.dumps(
            {'debug': True, 'pgo': False, 'artifact': False}))
        plan = {'criteria': {'macOSMin': '10.15'}}
        for state in ('valid', 'missing', 'wrong-architecture'):
            with self.subTest(state=state):
                current = dict(substs)
                if state == 'missing':
                    current.pop('HOST_CPU_ARCH')
                elif state == 'wrong-architecture':
                    current['HOST_CPU_ARCH'] = 'aarch64'
                (self.root / 'effective-config.private.log').write_text(json.dumps({'substs': current}))
                if state == 'valid':
                    verify_config(plan, self.root, tools, sdk)
                    record = json.loads((self.root / 'effective-config.json').read_text())
                    self.assertEqual(record['HOST_CPU_ARCH'], 'x86_64')
                    self.assertNotIn('HOST_CPU', record)
                else:
                    with self.assertRaisesRegex(Failure, 'compiled-config-host_cpu_arch'):
                        verify_config(plan, self.root, tools, sdk)

    def archive(self, entries):
        path = self.root / 'input.tar.gz'
        with tarfile.open(path, 'w:gz') as archive:
            for name, kind, data in entries:
                item = tarfile.TarInfo(name)
                item.type = kind
                if kind == tarfile.REGTYPE:
                    item.size = len(data)
                    archive.addfile(item, io.BytesIO(data))
                else:
                    item.linkname = data
                    archive.addfile(item)
        return path

    def test_reject_escaping_links_and_special_files(self):
        for kind, name, target in [(tarfile.SYMTYPE, 'SDK/link', '../../escape'),
                                    (tarfile.LNKTYPE, 'SDK/link', '../escape'),
                                    (tarfile.REGTYPE, '/outside', b'x'),
                                    (tarfile.FIFOTYPE, 'pipe', '')]:
            with self.subTest(name=name, kind=kind), self.assertRaises(Failure):
                scan_tar(self.archive([(name, kind, target)]))

    def test_reject_archive_ambiguity_and_size(self):
        with self.assertRaises(Failure):
            scan_tar(self.archive([('a', tarfile.REGTYPE, b'a'), ('./a', tarfile.REGTYPE, b'b')]))
        with self.assertRaises(Failure):
            scan_tar(self.archive([('a', tarfile.REGTYPE, b'ab')]), max_bytes=1)

    def test_extract_internal_framework_link_and_hardlink(self):
        path = self.archive([('SDK/Versions/A/file', tarfile.REGTYPE, b'checked'),
                             ('SDK/Versions/Current', tarfile.SYMTYPE, 'A'),
                             ('SDK/copy', tarfile.LNKTYPE, 'SDK/Versions/A/file')])
        extract_tar(path, self.root / 'output')
        self.assertEqual((self.root / 'output/SDK/Versions/Current/file').read_bytes(), b'checked')
        self.assertEqual((self.root / 'output/SDK/copy').read_bytes(), b'checked')

    def test_failure_cannot_be_replaced_by_cleanup_failure(self):
        first = Runner(self.root, time.time(), {})
        first.fail('compile', 'initial')
        later = Runner(self.root, time.time(), {})
        later.fail('cleanup', 'secondary')
        self.assertEqual(first.failure, later.failure)
        self.assertEqual(json.loads((self.root / 'first-failure.json').read_text())['stage'], 'compile')

    def test_first_failure_survives_evidence_storage_error(self):
        runner = Runner(self.root, time.time(), {})
        with mock.patch('support.os.link', side_effect=OSError(errno.ENOSPC, 'no space')):
            runner.fail('compile', 'initial')
        self.assertEqual(runner.failure['code'], 'initial')
        self.assertEqual(runner.failure['persistenceError'], errno.ENOSPC)

    def test_group_cleanup_does_not_stop_at_leader_exit(self):
        runner = Runner(self.root, time.time(), {'processCleanupGraceSeconds': 0})
        alive, signals = [True], []
        def killpg(_pid, sig):
            if not alive[0]:
                raise ProcessLookupError()
            if sig:
                signals.append(sig)
                alive[0] = False
        with mock.patch('support.os.killpg', side_effect=killpg):
            runner.stop(mock.Mock(pid=99999, poll=mock.Mock(return_value=0)))
        self.assertTrue(signals)
        self.assertFalse(alive[0])

    def test_deadline_uses_monotonic_origin(self):
        (self.root / 'started-monotonic').write_text(str(time.monotonic() - 10))
        runner = Runner(self.root, 0, {})
        with mock.patch('support.time.time', return_value=-10**10):
            self.assertGreaterEqual(runner.elapsed(), 10)
            self.assertLess(runner.elapsed(), 11)

    def owned_registry(self, current):
        registry = OwnedNative.__new__(OwnedNative)
        registry.task = self.root
        registry.finished, registry.proof = False, None
        original = {'pid': 99999, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 10,
                    'exe': '/private/test.app/browser', 'argvSHA256': 'a' * 64, 'kind': 'host'}
        registry.records = {(99999, 100, 10): original}
        registry.system = mock.Mock()
        registry.system.describe.side_effect = lambda _pid: current[0]
        registry.census = mock.Mock()
        return registry, original

    def test_native_cleanup_never_signals_reused_pid(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = {**original, 'startUsec': 11}
        with mock.patch('owned_native.os.kill') as send:
            result = registry.finish()
        send.assert_not_called()
        self.assertTrue(result['allOwnedGone'])

    def test_native_cleanup_signals_only_matching_owned_generation(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = original
        def send(pid, _sig):
            self.assertEqual(pid, original['pid'])
            current[0] = None
        with mock.patch('owned_native.os.kill', side_effect=send), mock.patch('owned_native.PASSIVE_SETTLE_SECONDS', 0):
            result = registry.finish()
        self.assertEqual(result['outlivedHarness'], 1)
        self.assertTrue(result['allOwnedGone'])

    def test_changed_arguments_of_same_generation_remain_unresolved(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = {**original, 'argvSHA256': 'b' * 64}
        with mock.patch('owned_native.os.kill') as send:
            result = registry.finish()
        send.assert_not_called()
        self.assertFalse(result['allOwnedGone'])
        self.assertIn('owned-generation-identity-changed', result['errors'])

    def test_census_error_does_not_skip_known_instance_cleanup(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = original
        registry.census.side_effect = Failure('unrecognized-helper')
        def send(_pid, _sig):
            current[0] = None
        with mock.patch('owned_native.os.kill', side_effect=send) as signal_owned, mock.patch('owned_native.PASSIVE_SETTLE_SECONDS', 0):
            result = registry.finish()
        signal_owned.assert_called_once()
        self.assertFalse(result['censusComplete'])
        self.assertFalse(result['allOwnedGone'])

    def test_native_census_binds_profile_and_never_persists_nonce(self):
        browser = self.root / 'validation.app'
        (browser / 'Contents').mkdir(parents=True)
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        private = self.root / 'native-private/cause'
        profile = private / 'profile'
        profile.mkdir(parents=True)
        shim = private / 'Runtime Smoke.app/Contents/MacOS/floorp-app-shim'
        shim.parent.mkdir(parents=True)
        (shim.parents[1] / 'Info.plist').write_bytes(plistlib.dumps({
            'FloorpAppShimHostPath': str(browser), 'FloorpAppShimProfilePath': str(profile)}))
        exe = browser / 'Contents/MacOS/browser'
        host_args = str(exe) + ' --no-remote --profile ' + str(profile) + ' --marionette --remote-allow-system-access about:blank'
        nonce = 'a' * 64
        shim_args = str(shim) + ' --host-service test --host-pid 42 --launch-token ' + nonce
        rows = {42: {'pid': 42, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 1, 'pgid': 1,
                     'exe': str(exe), 'argv': host_args, 'argvSHA256': '1' * 64},
                43: {'pid': 43, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 2, 'pgid': 1,
                     'exe': str(shim), 'argv': shim_args, 'argvSHA256': '2' * 64}}
        helper_relative = 'Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container'
        helper = browser / helper_relative
        rows[44] = {'pid': 44, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 3, 'pgid': 1,
                    'ppid': 42, 'exe': str(helper), 'argv': str(helper) + ' -contentproc', 'argvSHA256': '3' * 64}
        system = mock.Mock()
        system.pids.side_effect = lambda: list(rows)
        system.path.side_effect = lambda pid: rows[pid]['exe']
        system.describe.side_effect = lambda pid: rows.get(pid)
        with mock.patch('owned_native.MacProcesses', return_value=system):
            registry = OwnedNative(self.root, browser, self.root, 90, (helper_relative,))
        registry.census()
        self.assertEqual(len(registry.records), 3)
        self.assertNotIn(nonce, (self.root / 'owned-native.private.json').read_text())
        rows[44]['ppid'] = 1
        del rows[42]
        registry.census()
        self.assertIn(44, [row['pid'] for row in registry.live()])
        (shim.parents[1] / 'Info.plist').write_bytes(plistlib.dumps({
            'FloorpAppShimHostPath': str(browser), 'FloorpAppShimProfilePath': str(private / 'other')}))
        with self.assertRaises(Failure):
            registry.census()

    def test_native_child_environment_excludes_credentials_and_overrides(self):
        supplied = {'HOME': '/actual-user', 'SDK_READ_TOKEN': 'secret', 'GITHUB_TOKEN': 'secret',
                    'ACTIONS_RUNTIME_TOKEN': 'secret', 'MOZ_DISABLE_CONTENT_SANDBOX': '1',
                    'DYLD_LIBRARY_PATH': '/override', 'SSH_AUTH_SOCK': '/agent'}
        with mock.patch.dict(os.environ, supplied, clear=True):
            result = clean_environment(self.root)
        self.assertEqual(result['HOME'], '/actual-user')
        self.assertFalse(set(supplied) - {'HOME'} & set(result))

    def test_skip_untrusted_event_or_missing_exit_cannot_pass(self):
        value = {'status': 'passed', 'requestedIterations': 20, 'attemptedIterations': 20,
                 'customMenuVisible': True, 'shimPid': 42, 'shimLifecycle': {'events': [
                     {'kind': 'kernel-exit-observed', 'pid': 42}]}, 'results': [
                     {'type': 'contextmenu', 'constructor': 'PointerEvent', 'pointerType': 'mouse',
                      'button': 2, 'isTrusted': True} for _ in range(20)]}
        self.assertEqual(context_result(value)['trustedContextMenus'], 20)
        for mutate in (lambda v: v.update(status='partial'),
                       lambda v: v.update(attemptedIterations=0),
                       lambda v: v['results'][0].update(isTrusted=False),
                       lambda v: v['shimLifecycle'].update(events=[])):
            bad = copy.deepcopy(value)
            mutate(bad)
            with self.assertRaises(Failure):
                context_result(bad)

    def test_failure_projection_keeps_cause_but_drops_private_values(self):
        value = {'profile': 'PRIVATE', 'beforeCleanup': {'shim': {'events': [
            {'type': 'disconnected', 'reason': 'process-exited', 'causeReason': 'hello-timeout',
             'pid': 42, 'launchToken': 'PRIVATE', 'cookie': 'PRIVATE', 'causeTimeMs': 123}]}}}
        result = failure_snapshot(value)
        self.assertNotIn('PRIVATE', json.dumps(result))
        self.assertEqual(result['events'][0]['causeReason'], 'hello-timeout')

    def thin_macho(self, name, code=b'CODE', signature=b'signature'):
        command_size = 72 + 80 + 24
        offset = 32 + command_size
        header = struct.pack('<IIIIIIII', 0xFEEDFACF, 0x01000007, 3, 6, 2, command_size, 0, 0)
        segment = struct.pack('<II16sQQQQiiII', 0x19, 152, b'__TEXT', 0, 4096, 0, offset + len(code), 7, 5, 1, 0)
        section = struct.pack('<16s16sQQIIIIIIII', b'__text', b'__TEXT', 4096, len(code), offset, 2, 0, 0, 0x80000400, 0, 0, 0)
        uuid = struct.pack('<II', 0x1B, 24) + b'U' * 16
        path = self.root / name
        path.write_bytes(header + segment + section + uuid + code + signature)
        return path

    def test_signing_blob_difference_keeps_code_binding(self):
        before = macho(self.thin_macho('before'), 6)
        after = macho(self.thin_macho('after', signature=b'changed'), 6)
        changed = macho(self.thin_macho('changed', code=b'EVIL'), 6)
        self.assertNotEqual(before['sha256'], after['sha256'])
        self.assertEqual(before['sectionsSHA256'], after['sectionsSHA256'])
        self.assertNotEqual(before['sectionsSHA256'], changed['sectionsSHA256'])
        self.assertEqual(before['uuid'], after['uuid'])

    def test_wrong_architecture_or_type_cannot_bind(self):
        path = self.thin_macho('wrong')
        with self.assertRaises(Failure):
            macho(path, 1)
        data = bytearray(path.read_bytes())
        struct.pack_into('<I', data, 4, 0x0100000C)
        path.write_bytes(data)
        with self.assertRaises(Failure):
            macho(path, 6)


if __name__ == '__main__':
    unittest.main()
