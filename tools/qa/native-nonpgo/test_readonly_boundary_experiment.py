# SPDX-License-Identifier: MPL-2.0
import contextlib
import errno
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

import readonly_boundary_experiment as rb


def disk(mount, readonly=False):
    return {'MountPoint': str(mount), 'DeviceNode': '/dev/disk52s1',
        'FilesystemType': 'apfs', 'VolumeUUID': '01234567-0123-0123-0123-0123456789ab',
        'ReadOnlyVolume': readonly, 'ReadOnlyMedia': readonly}


def entry(mount):
    return {'system-entities': [{'dev-entry': '/dev/disk51'}, {'dev-entry': '/dev/disk52'},
        {'dev-entry': '/dev/disk52s1', 'mount-point': str(mount)}]}


class BindingTests(unittest.TestCase):
    def test_image_entry_only_exact_canonical_private_image(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = Path(temporary)/'private.sparseimage'
            value = {'image-path': str(image), **entry(Path(temporary)/'mount')}
            other = {'image-path': str(Path(temporary)/'OTHER')}
            self.assertIs(rb.image_entry({'images': [other, value]}, image), value)
            self.assertIsNone(rb.image_entry({'images': [other]}, image))
            with self.assertRaisesRegex(rb.ExperimentFailure, 'ambiguous'):
                rb.image_entry({'images': [value, value]}, image)
            alias = Path(temporary)/'directory-alias'
            alias.symlink_to(Path(temporary), target_is_directory=True)
            self.assertIs(rb.image_entry({'images': [value]}, alias/'private.sparseimage'), value)
            owner = rb.Image(alias, mock.Mock(), {'commands': [], 'bindings': []})
            self.assertEqual(owner.directory, Path(temporary).resolve())

    def test_volume_binding_requires_device_uuid_and_canonical_mount(self):
        mount = Path('/private/controlled/mount')
        value = rb.mount_binding(entry(mount), mount, disk(mount))
        self.assertEqual(value['detach'], '/dev/disk51')
        self.assertEqual(value['devices'], ('/dev/disk51', '/dev/disk52', '/dev/disk52s1'))
        for key, replacement in [('MountPoint', '/OTHER'), ('DeviceNode', '/dev/disk9'),
                                 ('VolumeUUID', None),
                                 ('FilesystemType', 'hfs')]:
            info = disk(mount); info[key] = replacement
            with self.subTest(key=key), self.assertRaises(rb.ExperimentFailure):
                rb.mount_binding(entry(mount), mount, info)

    def test_other_mount_malformed_device_duplicate_refused(self):
        mount = Path('/private/controlled/mount')
        for addition in [{'dev-entry': '/dev/disk9s1', 'mount-point': '/OTHER'},
                         {'dev-entry': '/dev/disk51'}, {'dev-entry': '/dev/rdisk0'},
                         {'dev-entry': '/dev/disk1;PRIVATE'}]:
            value = entry(mount); value['system-entities'].append(addition)
            with self.subTest(addition=addition), self.assertRaises(rb.ExperimentFailure):
                rb.mount_binding(value, mount, disk(mount))

    def test_backing_symlink_hardlink_and_allocated_limit_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); image = root/'image'; image.write_bytes(b'data')
            original = rb.backing_identity(image)
            self.assertEqual(original[:2], (image.stat().st_dev, image.stat().st_ino))
            link = root/'symlink'; link.symlink_to(image)
            with self.assertRaises(rb.ExperimentFailure): rb.backing_identity(link)
            other = root/'hardlink'; os.link(image, other)
            with self.assertRaises(rb.ExperimentFailure): rb.backing_identity(image)
            other.unlink()
            allocated = SimpleNamespace(st_mode=stat.S_IFREG | 0o600, st_uid=os.geteuid(),
                                        st_nlink=1, st_blocks=1)
            with mock.patch.object(rb, 'MAX_STORED', 0), \
                 mock.patch.object(Path, 'lstat', return_value=allocated), \
                 self.assertRaisesRegex(rb.ExperimentFailure, 'storage'):
                rb.backing_identity(image)

    def test_replaced_backing_refuses_before_query_or_detach(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'one'); image.backing = rb.backing_identity(image.image)
            replacement = Path(temporary)/'replacement'; replacement.write_bytes(b'two')
            replacement.replace(image.image)
            with self.assertRaisesRegex(rb.ExperimentFailure, 'changed'): image.detach()
            command.plist.assert_not_called(); command.run.assert_not_called()

    def test_uuid_change_or_unknown_mount_cannot_detach(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'one'); image.backing = rb.backing_identity(image.image)
            image.volume = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
            command.plist.side_effect = [{'images': [{'image-path': str(image.image), **entry(image.mount)}]}, disk(image.mount)]
            with self.assertRaisesRegex(rb.ExperimentFailure, 'uuid'): image.detach()
            command.run.assert_not_called()
            command.plist.side_effect = [{'images': [{'image-path': '/OTHER', **entry(image.mount)}]}]
            with self.assertRaisesRegex(rb.ExperimentFailure, 'not-attached'): image.detach()
            command.run.assert_not_called()

    def test_readonly_mismatch_refuses_and_binding_has_no_raw_identifiers(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'one'); image.backing = rb.backing_identity(image.image)
            values = [{'images': [{'image-path': str(image.image), **entry(image.mount)}]}, disk(image.mount)]
            command.plist.side_effect = values*2
            with mock.patch.object(rb.os, 'statvfs', return_value=SimpleNamespace(f_flag=0)), \
                 self.assertRaisesRegex(rb.ExperimentFailure, 'readonly'):
                image.verify(readonly=True)
            command.plist.side_effect = values*2
            with mock.patch.object(rb.os, 'statvfs', return_value=SimpleNamespace(f_flag=0)):
                image.verify(readonly=False)
            receipt = json.dumps(result)
            for private in (temporary, '/dev/disk51', disk(image.mount)['VolumeUUID']):
                self.assertNotIn(private, receipt)

    def test_held_detach_nonzero_is_never_asserted_ebusy(self):
        result = {'commands': [], 'bindings': []}; command = mock.Mock()
        with tempfile.TemporaryDirectory() as temporary:
            image = rb.Image(Path(temporary), command, result)
            image.verify = mock.Mock(return_value={'detach': '/dev/disk51'})
            image.verify_ownership = mock.Mock(return_value={'detach': '/dev/disk51'})
            image.entry = mock.Mock(return_value={})
            def refuse(*_args, **_kwargs):
                result['commands'].append({'stage': 'image-detach-held', 'busyTextSeen': False})
                return 1, b''
            command.run.side_effect = refuse
            value = image.detach(held=True)
            self.assertEqual(value['outcome'], 'refused')
            self.assertTrue(value['originalMountContinues'])
            self.assertFalse(value['busyTextSeen']); self.assertFalse(value['busyErrnoEstablished'])
            self.assertIsNone(value['oldHandleWriteAfterDetach'])
            self.assertEqual(image.verify.call_count, 1)
            self.assertEqual(image.verify_ownership.call_count, 1)
            self.assertNotIn('-force', command.run.call_args.args[1])

    def test_successful_held_detach_preserves_unknown_old_handle(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = rb.Image(Path(temporary), mock.Mock(), {'commands': []})
            image.verify = mock.Mock(return_value={'detach': '/dev/disk51'})
            image.verify_ownership = mock.Mock(return_value={'detach': '/dev/disk51'})
            image.entry = mock.Mock(return_value=None)
            image.commands.run.return_value = (0, b'')
            with mock.patch.object(rb.os.path, 'ismount', return_value=False):
                value = image.detach(held=True)
            self.assertEqual(value['outcome'], 'detached')
            self.assertIsNone(value['oldHandleWriteAfterDetach'])
            self.assertNotIn('safeFreeze', value)

    def test_create_failure_without_backing_reports_unknown_and_never_detaches(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); command.run.return_value = (1, b'PRIVATE')
            image = rb.Image(Path(temporary), command, {'commands': [], 'bindings': []})
            with self.assertRaisesRegex(rb.ExperimentFailure, 'create-failed'):
                image.create()
            value = image.cleanup()
            self.assertEqual(value['attachmentState'], 'unknown')
            self.assertFalse(value['cleanupVerified'])
            self.assertEqual(value['failure']['code'], 'owned-image-cleanup-unverified')
            self.assertIsNone(value['normalDetach'])
            self.assertEqual(command.run.call_count, 1)
            command.plist.assert_not_called()
            self.assertNotIn('PRIVATE', json.dumps(value))

    def test_attach_failure_unbindable_entities_stay_unknown_without_detach(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'own'); image.backing = rb.backing_identity(image.image)
            # The exact backing may be attached, but an unrelated mount defeats ownership binding.
            foreign = {'image-path': str(image.image), **entry(Path('/OTHER'))}
            command.plist.side_effect = [
                {'images': [foreign]}, {'images': [foreign]}, disk(image.mount)]
            value = image.cleanup()
            self.assertEqual(value['attachmentState'], 'unknown')
            self.assertFalse(value['cleanupVerified']); self.assertIsNotNone(value['failure'])
            command.run.assert_not_called()

    def test_known_property_projection_has_types_without_coercion_or_private_values(self):
        supplied = {'ReadOnlyVolume': False, 'ReadOnlyMedia': 'PRIVATE-STRING',
            'Writable': 0, 'VolumeReadOnly': True, 'MediaWritable': ['PRIVATE-LIST'],
            'UNKNOWN-PRIVATE-KEY': {'PRIVATE-PATH': '/PRIVATE'}}
        value = rb.readonly_properties(supplied)
        self.assertEqual(set(value), set(rb.DISK_READONLY_KEYS))
        self.assertEqual(value['ReadOnlyVolume'], {'present': True, 'type': 'boolean', 'value': False})
        self.assertEqual(value['ReadOnlyMedia'], {'present': True, 'type': 'string', 'value': None})
        self.assertEqual(value['Writable'], {'present': True, 'type': 'integer', 'value': None})
        self.assertEqual(value['VolumeWritable'], {'present': False, 'type': 'absent', 'value': None})
        self.assertEqual(value['VolumeReadOnly']['value'], True)
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_missing_readonly_keys_owned_identity_cleanup_succeeds_without_state_probe(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'own'); image.backing = rb.backing_identity(image.image)
            info = {'images': [{'image-path': str(image.image), **entry(image.mount)}]}
            identity_disk = {key: value for key, value in disk(image.mount).items()
                             if key not in rb.DISK_READONLY_KEYS}
            command.plist.side_effect = [info, info, identity_disk, {'images': []}]
            command.run.return_value = (0, b'')
            with mock.patch.object(rb.os, 'statvfs', side_effect=PermissionError('PRIVATE')) as state, \
                 mock.patch.object(rb.os.path, 'ismount', return_value=False):
                value = image.cleanup()
            state.assert_not_called()
            self.assertTrue(value['cleanupVerified']); self.assertIsNone(value['failure'])
            self.assertFalse(value['readOnlyPropertiesRequired']); self.assertFalse(value['statvfsRequired'])
            self.assertEqual(value['attachmentState'], 'verified-not-attached')
            self.assertEqual(command.run.call_args.args[1], [rb.HDIUTIL, 'detach', '/dev/disk51'])
            properties = result['bindings'][0]['diskReadOnlyProperties']
            self.assertTrue(all(item['present'] is False and item['value'] is None for item in properties.values()))
            self.assertIsNone(result['bindings'][0]['diskutilReadOnlyConclusion'])

    def test_missing_identity_still_forbids_cleanup_even_with_state_keys(self):
        for key in ('DeviceNode', 'MountPoint', 'VolumeUUID'):
            with self.subTest(key=key), tempfile.TemporaryDirectory() as temporary:
                command = mock.Mock(); result = {'commands': [], 'bindings': []}
                image = rb.Image(Path(temporary), command, result)
                image.image.write_bytes(b'own'); image.backing = rb.backing_identity(image.image)
                info = {'images': [{'image-path': str(image.image), **entry(image.mount)}]}
                details = disk(image.mount); details.pop(key)
                command.plist.side_effect = [info, info, details]
                value = image.cleanup()
                self.assertFalse(value['cleanupVerified']); self.assertIsNotNone(value['failure'])
                self.assertEqual(value['attachmentState'], 'unknown'); command.run.assert_not_called()
                self.assertEqual(result['bindings'][0]['ownershipStatus'], 'unavailable')
                self.assertTrue(result['bindings'][0]['diskPropertiesObserved'])

    def test_kernel_ro_fixture_controls_keep_diskutil_all_fields_diagnostic_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'own'); image.backing = rb.backing_identity(image.image)
            info = {'images': [{'image-path': str(image.image), **entry(image.mount)}]}
            details = disk(image.mount)
            details.update(ReadOnlyVolume=False, ReadOnlyMedia=False, Writable=True)
            command.plist.side_effect = [info, details]*2
            with mock.patch.object(rb.os, 'statvfs', return_value=SimpleNamespace(f_flag=rb.os.ST_RDONLY)):
                image.verify(readonly=True)
            self.assertEqual(len(result['bindings']), 2)
            for value in result['bindings']:
                self.assertTrue(value['statvfsReadOnly']); self.assertTrue(value['kernelMountBindingVerified'])
                self.assertTrue(value['diskutilPropertiesDiagnosticOnly'])
                self.assertIsNone(value['diskutilReadOnlyConclusion'])
                self.assertIs(value['diskReadOnlyProperties']['ReadOnlyMedia']['value'], False)
            self.assertNotIn(temporary, json.dumps(result))

    def test_unknown_kernel_flags_remain_unknown(self):
        for flags in (None, True, '1', -1, 2**64):
            observation = {}
            with self.subTest(flags=flags), \
                 mock.patch.object(rb.os, 'statvfs', return_value=SimpleNamespace(f_flag=flags)), \
                 self.assertRaisesRegex(rb.ExperimentFailure, 'kernel-flags-unavailable'):
                rb.kernel_readonly(Path('/PRIVATE'), observation)
            self.assertIsNone(observation['statvfsReadOnly'])
            self.assertEqual(observation['statvfsStatus'], 'unavailable')

    def test_changed_mount_state_fails_after_bracketed_identity_checks(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = mock.Mock(); result = {'commands': [], 'bindings': []}
            image = rb.Image(Path(temporary), command, result)
            image.image.write_bytes(b'own'); image.backing = rb.backing_identity(image.image)
            info = {'images': [{'image-path': str(image.image), **entry(image.mount)}]}
            command.plist.side_effect = [info, disk(image.mount)]*2
            with mock.patch.object(rb.os, 'statvfs', side_effect=[SimpleNamespace(f_flag=1), SimpleNamespace(f_flag=0)]), \
                 self.assertRaisesRegex(rb.ExperimentFailure, 'kernel-readonly-state-changed'):
                image.verify(readonly=True)
            self.assertEqual(len(result['bindings']), 2)
            self.assertFalse(result['bindings'][-1]['kernelMountBindingVerified'])
            self.assertTrue(all(item['ownershipStatus'] == 'verified' for item in result['bindings']))


class UtilityTests(unittest.TestCase):
    def test_actual_utility_inherits_outer_process_group(self):
        commands = rb.Commands({'commands': []})
        with mock.patch.object(rb, 'HDIUTIL', sys.executable):
            code, output = commands.run('group-inheritance', [sys.executable, '-c',
                'import os;print(os.getpgrp())'])
        self.assertEqual(code, 0); self.assertEqual(int(output), os.getpgrp())
        cleanup = commands.result['commands'][0]['cleanup']
        self.assertIsNone(cleanup['groupGone'])
        self.assertEqual(cleanup['groupScope'], 'inherited-outer-monitored-stage')

    def test_unsafe_options_or_unlisted_utility_never_spawn(self):
        commands = rb.Commands({'commands': []})
        for args in [['/bin/rm', '-rf', '/'], [rb.HDIUTIL, 'detach', '-force'],
                     [rb.HDIUTIL, 'create', '-encryption']]:
            with mock.patch.object(rb.subprocess, 'Popen') as popen, self.assertRaises(rb.ExperimentFailure):
                commands.run('unsafe', args)
            popen.assert_not_called()

    def test_output_limit_reaps_exact_child_and_hides_utility_text(self):
        result = {'commands': []}; commands = rb.Commands(result)
        with mock.patch.object(rb, 'HDIUTIL', sys.executable), mock.patch.object(rb, 'MAX_OUTPUT', 64), \
             self.assertRaisesRegex(rb.ExperimentFailure, 'output-limit'):
            commands.run('output', [sys.executable, '-c', "print('PRIVATE-CANARY-'*1000)"])
        self.assertLessEqual(commands.output_bytes, 64)
        record = result['commands'][0]
        self.assertTrue(record['cleanup']['leaderReaped']); self.assertIsNone(record['cleanup']['groupGone'])
        self.assertNotIn('PRIVATE-CANARY', json.dumps(result))

    def test_deadline_is_bounded_and_original_failure_not_replaced(self):
        result = {'commands': []}; commands = rb.Commands(result)
        started = time.monotonic()
        with mock.patch.object(rb, 'HDIUTIL', sys.executable), mock.patch.object(rb, 'COMMAND_SECONDS', .15), \
             self.assertRaisesRegex(rb.ExperimentFailure, 'utility-deadline'):
            commands.run('timeout', [sys.executable, '-c', 'import time;time.sleep(30)'])
        self.assertLess(time.monotonic()-started, 4)
        self.assertTrue(result['commands'][0]['cleanup']['leaderReaped'])
        original_reap = rb.reap
        def failed_reap(process, **kwargs):
            value = original_reap(process, **kwargs); value['failure'] = {'kind': 'cleanup-denied', 'errno': 1}; return value
        with mock.patch.object(rb, 'HDIUTIL', sys.executable), mock.patch.object(rb, 'MAX_OUTPUT', 0), \
             mock.patch.object(rb, 'reap', side_effect=failed_reap), \
             self.assertRaisesRegex(rb.ExperimentFailure, 'output-limit'):
            commands.run('both-fail', [sys.executable, '-c', "print('x')"])
        self.assertEqual(result['commands'][-1]['cleanup']['failure']['errno'], 1)

    def test_plist_nonzero_does_not_decode_or_publish_arbitrary_text(self):
        result = {'commands': []}; commands = rb.Commands(result)
        with mock.patch.object(commands, 'run', return_value=(1, b'PRIVATE')):
            with self.assertRaisesRegex(rb.ExperimentFailure, 'nonzero'):
                commands.plist('bad', [rb.HDIUTIL, 'info', '-plist'])
        self.assertNotIn('PRIVATE', json.dumps(result))


class RealChildTests(unittest.TestCase):
    def test_actual_fd_and_fd_closed_shared_mapping_write_and_natural_eof(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'payload'
            for kind, expected in [('fd', b'F'), ('mapping', b'M')]:
                with self.subTest(kind=kind):
                    path.write_bytes(b'0'*4096)
                    child = rb.Child(kind, path, rb.Commands({'commands': []}))
                    try:
                        self.assertEqual(os.getpgid(child.process.pid), os.getpgrp())
                        ready = child.read()
                        self.assertTrue(ready['sameUID']); self.assertTrue(ready['targetOpened'])
                        self.assertIs(ready['fdClosed'], kind == 'mapping')
                        self.assertEqual(child.command(), {'writeCompleted': True})
                        self.assertEqual(path.read_bytes()[:1], expected)
                    finally:
                        cleanup = rb.reap(child.process, natural=True)
                    self.assertEqual(cleanup['exitCode'], 0)
                    self.assertTrue(cleanup['leaderReaped']); self.assertIsNone(cleanup['groupGone'])
                    self.assertEqual(cleanup['signalsSent'], []); self.assertIsNone(cleanup['failure'])

    def test_actual_late_child_does_not_open_target_before_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            child = rb.Child('late', Path(temporary)/'not-created', rb.Commands({'commands': []}))
            try:
                ready = child.read()
                self.assertTrue(ready['sameUID']); self.assertFalse(ready['targetOpened'])
                self.assertIsNone(child.process.poll())
            finally:
                value = rb.reap(child.process, natural=True)
            self.assertEqual(value['exitCode'], 0); self.assertIsNone(value['groupGone'])

    def test_unresponsive_child_has_timed_read_and_cleanup(self):
        child = object.__new__(rb.Child)
        child.commands = rb.Commands({'commands': []}); child.commands.deadline = time.monotonic()+.15
        child.buffer, child.bytes = bytearray(), 0
        child.process = subprocess.Popen([sys.executable, '-c', 'import time;time.sleep(30)'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            with self.assertRaisesRegex(rb.ExperimentFailure, 'deadline'): child.read()
        finally:
            value = rb.reap(child.process)
        self.assertTrue(value['leaderReaped']); self.assertIsNone(value['groupGone'])

    def test_census_missing_is_explicit_unknown_and_no_fallback(self):
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch('checkpoint.owned_processes') as owned:
            baseline, receipt = rb.baseline_for(mock.Mock())
        owned.assert_not_called(); self.assertIsNone(baseline)
        self.assertIsNone(receipt['childIncludedInBaseline']); self.assertIsNone(receipt['childNewCandidate'])

    def test_census_baseline_keeps_same_child_and_unavailable_is_failure(self):
        row = (987654321, 123, 456, os.geteuid(), os.getuid()); child = mock.Mock()
        child.process.pid = row[0]; child.process.poll.return_value = None
        with mock.patch.dict(os.environ, {'FLOORP_CENSUS_ABI': 'PRIVATE'}), \
             mock.patch('checkpoint.owned_processes', return_value={row}):
            baseline, receipt = rb.baseline_for(child)
            rb.verify_baseline(child, baseline, receipt)
        self.assertTrue(receipt['childStillSameBaselineIdentity']); self.assertFalse(receipt['childNewCandidate'])
        self.assertNotIn(str(row[0]), json.dumps(receipt))
        with mock.patch('checkpoint.owned_processes', return_value=set()), self.assertRaises(rb.ExperimentFailure):
            rb.verify_baseline(child, baseline, receipt)
        with mock.patch.dict(os.environ, {'FLOORP_CENSUS_ABI': 'PRIVATE'}), \
             mock.patch('checkpoint.owned_processes', side_effect=PermissionError('SECRET')):
            with self.assertRaises(PermissionError): rb.baseline_for(child)


class ExperimentTests(unittest.TestCase):
    def test_linux_is_explicitly_unverified_and_never_runs_utility(self):
        result = {}
        with mock.patch.object(rb.sys, 'platform', 'linux'), mock.patch.object(rb.subprocess, 'Popen') as popen:
            rb.run_experiment(Path('/PRIVATE'), result)
        popen.assert_not_called()
        self.assertEqual(result['firstFailure']['code'], 'readonly-experiment-platform')
        self.assertEqual(result['qualification'], 'NO_GO')
        self.assertFalse(result['checkpointEligibilityEstablished']); self.assertNotIn('/PRIVATE', json.dumps(result))

    def test_procedure_completed_still_cannot_qualify(self):
        with tempfile.TemporaryDirectory() as temporary:
            fake = mock.Mock(); fake.mount = Path(temporary)
            fake.cleanup.return_value = {'normalDetach': 'verified-not-attached', 'failure': None}
            result = {}
            with mock.patch.object(rb.sys, 'platform', 'darwin'), mock.patch.object(rb, 'Image', return_value=fake), \
                 mock.patch.object(rb, 'writable_control') as writable, mock.patch.object(rb, 'readonly_control'):
                rb.run_experiment(Path(temporary), result)
        self.assertEqual(result['status'], 'completed'); self.assertEqual(writable.call_count, 2)
        self.assertEqual(result['qualification'], 'NO_GO')
        for flag in ('productionGateReplacement', 'checkpointEligibilityEstablished', 'sameUIDImmutabilityEstablished'):
            self.assertFalse(result[flag])
        self.assertTrue(result['allPayloadAndRefsSingleFreezeRequired'])

    def test_cleanup_failure_retained_separately_from_original_private_exception(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = mock.Mock(); image.create.side_effect = PermissionError(errno.EACCES, 'SECRET')
            image.last_binding = None
            image.cleanup.return_value = {'failure': {'kind': 'PermissionError', 'errno': 1}}
            result = {}
            with mock.patch.object(rb.sys, 'platform', 'darwin'), mock.patch.object(rb, 'Image', return_value=image):
                rb.run_experiment(Path(temporary), result)
        self.assertEqual(result['firstFailure']['stage'], 'create-attach')
        self.assertEqual(result['firstFailure']['errno'], errno.EACCES)
        self.assertEqual(result['imageCleanup']['failure']['errno'], 1)
        self.assertEqual(result['status'], 'failed'); self.assertNotIn('SECRET', json.dumps(result))

    def test_stop_schema_receipt_is_frozen_before_cleanup_observation_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = mock.Mock(); image.last_binding = {'ownershipStatus': 'verified',
                'diskReadOnlyProperties': rb.readonly_properties({'ReadOnlyVolume': 'PRIVATE'})}
            image.attach.side_effect = rb.ExperimentFailure('readonly-state-mismatch')
            def cleanup():
                image.last_binding['diskReadOnlyProperties'] = None
                return {'failure': None, 'cleanupVerified': True}
            image.cleanup.side_effect = cleanup
            result = {}
            with mock.patch.object(rb.sys, 'platform', 'darwin'), mock.patch.object(rb, 'Image', return_value=image):
                rb.run_experiment(Path(temporary), result)
        stop = result['stopBindingObservation']['diskReadOnlyProperties']
        self.assertEqual(stop['ReadOnlyVolume']['type'], 'string')
        self.assertIsNone(stop['ReadOnlyVolume']['value'])
        self.assertEqual(result['firstFailure']['code'], 'readonly-state-mismatch')
        self.assertTrue(result['imageCleanup']['cleanupVerified'])
        self.assertEqual(result['qualification'], 'NO_GO'); self.assertNotIn('PRIVATE', json.dumps(result))

    def test_cli_emits_safe_failure_and_exclusive_private_result(self):
        with tempfile.TemporaryDirectory() as temporary:
            argv = ['readonly_boundary_experiment.py', '--task', temporary]
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(rb.sys, 'platform', 'linux'), \
                 contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(rb.main(), 1)
            path = Path(temporary)/'readonly-boundary-experiment.json'
            value = json.loads(path.read_text())
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
            self.assertEqual(value, json.loads(output.getvalue())['readonlyBoundaryExperiment'])
            self.assertNotIn(temporary, output.getvalue())
            before = path.read_bytes()
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(rb.sys, 'platform', 'linux'), \
                 contextlib.redirect_stdout(io.StringIO()) as second:
                self.assertEqual(rb.main(), 1)
            self.assertEqual(path.read_bytes(), before)
            self.assertIn('evidenceWriteFailure', json.loads(second.getvalue())['readonlyBoundaryExperiment'])


if __name__ == '__main__':
    unittest.main()
