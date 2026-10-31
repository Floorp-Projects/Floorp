# SPDX-License-Identifier: MPL-2.0
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

import nss_dynamic_closure as nss


def dylib(uuid=b'a' * 16, cpu=0x01000007, kind=6):
    return struct.pack('<8I', 0xfeedfacf, cpu, 3, kind, 1, 24, 0, 0) + struct.pack('<2I', 0x1b, 24) + uuid


class DynamicClosureTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='nss-closure-')
        self.addCleanup(temporary.cleanup)
        self.task = Path(temporary.name).resolve()
        nodes = ['js/xpconnect/shell/target', 'toolkit/library/build/target',
                 'toolkit/library/rust/target-objects', 'widget/cocoa/target-objects',
                 *nss.DYNAMIC_TARGETS.values()]
        self.root = ('compile_targets := ' + ' '.join(nodes) + '\nmisc_dirs := security\n'
                     'libs_dirs := security\ntools_dirs := security\ninclude root-deps.mk\n').encode()
        self.deps = (b'js/xpconnect/shell/target: toolkit/library/build/target widget/cocoa/target-objects\n'
                     b'toolkit/library/build/target: toolkit/library/rust/target-objects\n')
        self.pins = {}
        for label, raw in (('rootMk', self.root), ('rootDepsMk', self.deps)):
            relative, cap, _ = nss.GRAPH_PINS[label]
            path = self.task / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            self.pins[label] = relative, cap, hashlib.sha256(raw).hexdigest()
        for prefix in ('obj/dist/bin', 'obj/dist/Test.app/Contents/MacOS'):
            (self.task / prefix).mkdir(parents=True)
            for name in nss.LIBRARIES:
                (self.task / prefix / name).write_bytes(dylib())

    def observe(self, **kwargs):
        with mock.patch.object(nss, 'GRAPH_PINS', self.pins):
            return nss.observe(kwargs.get('task', self.task), kwargs.get('bundle', 'Test.app'))

    def test_complete_snapshot_hashes_and_uuid_never_establish_real_load_or_qualification(self):
        value = self.observe()
        self.assertEqual(value['status'], 'observed')
        self.assertTrue(value['bindingsStable'])
        self.assertTrue(value['graphPinsMatch'])
        self.assertTrue(value['allLibrariesObserved'])
        self.assertTrue(value['bundleMatchesDist'])
        self.assertEqual(len(value['files']), 8)
        self.assertEqual(value['files']['dist:libsoftokn3.dylib']['macho']['uuid'], (b'a' * 16).hex())
        for name in ('realLoadPerformed', 'buildPerformed', 'productQualification', 'native42ResultQualification'):
            self.assertIs(value[name], False)

    def test_declared_dlopen_modules_can_be_absent_from_the_shell_link_closure(self):
        graph = self.observe()['graph']
        self.assertEqual(graph['targetsDeclared'], {'softokn3': True, 'freebl3': True})
        self.assertEqual(graph['shellReachesTargets'], {'softokn3': False, 'freebl3': False})

    def test_dynamic_edge_is_observed_only_when_literally_present(self):
        deps = self.deps + ('toolkit/library/build/target: ' + nss.DYNAMIC_TARGETS['softokn3'] + '\n').encode()
        graph = nss.graph_facts(self.root, deps)
        self.assertEqual(graph['shellReachesTargets'], {'softokn3': True, 'freebl3': False})

    def test_make_expressions_are_never_executed_or_promoted_to_literal_edges(self):
        sentinel = self.task / 'sentinel'
        deps = self.deps + ('security: $(shell touch ' + str(sentinel) + ')\n').encode()
        self.assertEqual(nss.graph_facts(self.root, deps)['status'], 'unconfirmed')
        self.assertFalse(sentinel.exists())

    def test_changed_graph_remains_explicitly_unpinned(self):
        (self.task / 'obj/root.mk').write_bytes(self.root + b'# changed\n')
        value = self.observe()
        self.assertFalse(value['graphPinsMatch'])
        self.assertTrue(value['bindingsStable'])

    def test_missing_library_preserves_every_other_observation_and_unknown_match(self):
        (self.task / 'obj/dist/Test.app/Contents/MacOS/libsoftokn3.dylib').unlink()
        value = self.observe()
        self.assertEqual(value['files']['bundle:libsoftokn3.dylib']['reason'], 'missing')
        self.assertTrue(value['bindingsStable'])
        self.assertFalse(value['allLibrariesObserved'])
        self.assertIsNone(value['bundleMatchesDist'])
        self.assertEqual(value['files']['dist:libsoftokn3.dylib']['status'], 'observed')

    def test_bundle_byte_mismatch_is_retained_even_with_valid_macho(self):
        (self.task / 'obj/dist/Test.app/Contents/MacOS/libfreebl3.dylib').write_bytes(dylib(b'b' * 16))
        value = self.observe()
        self.assertTrue(value['allLibrariesObserved'])
        self.assertIs(value['bundleMatchesDist'], False)

    def test_symlink_hardlink_and_fifo_are_not_read_as_owned_regular_inodes(self):
        path = self.task / 'obj/dist/bin/libsoftokn3.dylib'
        reference = self.task / 'reference'
        reference.write_bytes(b'PRIVATE_SENTINEL')
        for kind in ('symlink', 'hardlink', 'fifo'):
            path.unlink()
            if kind == 'symlink':
                path.symlink_to(reference)
            elif kind == 'hardlink':
                os.link(reference, path)
            else:
                os.mkfifo(path)
            value = self.observe()
            self.assertEqual(value['files']['dist:libsoftokn3.dylib']['reason'], 'file-kind')
            self.assertFalse(value['allLibrariesObserved'])
            self.assertNotIn('PRIVATE_SENTINEL', json.dumps(value))

    def test_symlinked_directory_is_not_traversed(self):
        original = self.task / 'obj/dist/bin'
        original.rename(self.task / 'binaries')
        original.symlink_to(self.task / 'binaries')
        value = self.observe()
        self.assertEqual(value['files']['dist:libnss3.dylib']['reason'], 'directory-binding')

    def test_final_fd_identity_detects_same_size_replacement(self):
        original = nss._FixedRead.verify
        path = self.task / 'obj/dist/bin/libsoftokn3.dylib'
        def replace_then_verify(reader):
            replacement = path.with_suffix('.new')
            replacement.write_bytes(path.read_bytes())
            replacement.replace(path)
            original(reader)
        with mock.patch.object(nss._FixedRead, 'verify', replace_then_verify):
            value = self.observe()
        self.assertEqual(value['status'], 'unconfirmed')
        self.assertFalse(value['bindingsStable'])

    def test_final_missing_path_binding_detects_a_file_created_during_read(self):
        path = self.task / 'obj/dist/bin/libsoftokn3.dylib'
        path.unlink()
        original = nss._FixedRead.verify
        def create_then_verify(reader):
            path.write_bytes(dylib())
            original(reader)
        with mock.patch.object(nss._FixedRead, 'verify', create_then_verify):
            value = self.observe()
        self.assertEqual(value['status'], 'unconfirmed')
        self.assertFalse(value['bindingsStable'])

    def test_unknown_root_and_bundle_are_rejected_without_paths_in_evidence(self):
        for bundle in ('../Other.app', '/private/Other.app', 'Other/Bad.app', 'Bad\n.app', None):
            value = self.observe(bundle=bundle)
            self.assertEqual(value['reason'], 'bundle-unconfirmed')
            self.assertEqual(value['files'], {})
        value = self.observe(task=self.task / '..' / self.task.name)
        self.assertEqual(value['status'], 'unconfirmed')
        self.assertNotIn(str(self.task), json.dumps(value))

    def test_malformed_macho_wrong_cpu_duplicate_uuid_and_truncated_command_are_unknown(self):
        valid = dylib()
        duplicate = struct.pack('<8I', 0xfeedfacf, 0x01000007, 3, 6, 2, 48, 0, 0) + valid[32:] * 2
        for raw in (b'', valid[:31], valid[:-1], dylib(cpu=0x0100000c), dylib(kind=2), duplicate,
                    valid[:36] + struct.pack('<I', 0) + valid[40:]):
            self.assertIsNone(nss.macho_identity(raw))

    def test_private_content_and_absolute_paths_are_never_returned(self):
        raw = dylib() + b'PRIVATE_SENTINEL'
        (self.task / 'obj/dist/bin/libnss3.dylib').write_bytes(raw)
        text = json.dumps(self.observe())
        self.assertNotIn('PRIVATE_SENTINEL', text)
        self.assertNotIn(str(self.task), text)

    def test_per_file_size_cap_rejects_a_sparse_oversize_library(self):
        path = self.task / 'obj/dist/bin/libsoftokn3.dylib'
        with path.open('wb') as stream:
            stream.truncate(16 * 1024**2 + 1)
        value = self.observe()
        self.assertEqual(value['files']['dist:libsoftokn3.dylib']['reason'], 'size-limit')
        self.assertFalse(value['allLibrariesObserved'])


if __name__ == '__main__':
    unittest.main()
