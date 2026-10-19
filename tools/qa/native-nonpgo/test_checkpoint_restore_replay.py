# SPDX-License-Identifier: MPL-2.0
import copy
import json
import os
import time
import unittest
from unittest import mock

import checkpoint as cp
import checkpoint_replay as replay
import test_checkpoint as fixture
from support import Failure


class ReplayRestoreIntegration(unittest.TestCase):
    setUp = fixture.CheckpointGuards.setUp
    save_plan = fixture.CheckpointGuards.save_plan
    pack = fixture.CheckpointGuards.pack
    consumer = fixture.CheckpointGuards.consumer
    inspected_manifest = fixture.CheckpointGuards.inspected_manifest
    assert_exact_restored_manifest = fixture.CheckpointGuards.assert_exact_restored_manifest

    def prepare_replay(self):
        self.config = replay.load_config()
        origin = self.config['origin']
        os.environ.update({replay.ENV_KEYS[key]:origin[key] for key in replay.HISTORIC_KEYS})
        os.environ['MOZ_BUILD_DATE']=self.config['buildDate']
        # Exercise the formerly broken multi-link chain in a complete archive.
        (self.task/'obj/forward-a').symlink_to('forward-z')
        (self.task/'obj/forward-z').symlink_to(self.source/'host.h')
        ready=self.pack()
        manifest=self.inspected_manifest(ready)
        origin.update(bytes=ready['bytes'],sha256=ready['sha256'],manifestSHA256=ready['manifestSHA256'])
        ready.update(artifact=origin['artifact'],producerJob=origin['producerJob'])
        os.environ.update(GITHUB_RUN_ID='991122',GITHUB_SHA='e'*40,FLOORP_FIXTURE_SHA='f'*40)
        ready['replay']={'contract':self.config['contract'],'origin':copy.deepcopy(origin),
                         'currentExecution':{key:os.environ[replay.ENV_KEYS[key]] for key in replay.EXECUTION_KEYS}}
        self.consumer(ready)
        return ready,manifest

    def restore_replay(self, *, opt_in=True):
        # Synthetic tiny archive pins replace only payload size/hashes; the
        # production origin/config constants are independently tested unpatched.
        with mock.patch.object(replay,'load_config',return_value=copy.deepcopy(self.config)), \
                mock.patch.dict(replay.ORIGIN,self.config['origin']):
            return cp.restore(self.plan_path,self.source,self.task,self.sdk,time.monotonic()+20,
                              replay_origin=opt_in)

    def test_full_tiny_archive_replay_preserves_all_rows_and_real_execution(self):
        _,manifest=self.prepare_replay()
        before=dict(os.environ)
        result=self.restore_replay()
        self.assert_exact_restored_manifest(manifest)
        self.assertEqual(result['qualification'],replay.CONTRACT)
        self.assertEqual(result['replay']['origin']['run'],'39197008470')
        self.assertEqual(result['replay']['currentExecution']['run'],'991122')
        self.assertEqual(dict(os.environ),before)
        evidence=json.loads((self.task/'checkpoint-replay-context.json').read_text())
        self.assertEqual(evidence['status'],'passed')
        self.assertTrue(all(evidence['keyMatches'].values()))
        self.assertNotIn(str(self.root),json.dumps(evidence))

    def test_replay_receipt_without_explicit_opt_in_is_rejected(self):
        self.prepare_replay()
        with self.assertRaisesRegex(Failure,'checkpoint-replay-opt-in'):
            self.restore_replay(opt_in=False)
        self.assertFalse((self.task/'obj').exists())

    def test_changed_nonhistoric_context_fails_before_extraction_and_emits_safe_bools(self):
        self.prepare_replay()
        os.environ['ImageVersion']='incompatible-image'
        with self.assertRaisesRegex(Failure,'checkpoint-replay-context-incompatible'):
            self.restore_replay()
        self.assertFalse((self.task/'obj').exists())
        evidence=json.loads((self.task/'checkpoint-replay-context.json').read_text())
        self.assertEqual(evidence['status'],'failed')
        self.assertIs(evidence['keyMatches']['image'],False)
        self.assertNotIn('incompatible-image',json.dumps(evidence))

    def test_receipt_payload_provenance_fields_must_equal_the_origin(self):
        ready,_=self.prepare_replay()
        ready['producerJob']+=1
        (self.task/'checkpoint-download.json').write_text(json.dumps(ready))
        with self.assertRaisesRegex(Failure,'checkpoint-replay-download-binding'):
            self.restore_replay()
        self.assertFalse((self.task/'obj').exists())

    def test_diagnostic_write_failure_preserves_original_context_failure(self):
        self.prepare_replay()
        os.environ['ImageVersion']='incompatible-image'
        with mock.patch.object(cp,'write_json',side_effect=OSError('PRIVATE-SENTINEL')), \
                self.assertRaisesRegex(Failure,'checkpoint-replay-context-incompatible'):
            self.restore_replay()
        self.assertFalse((self.task/'obj').exists())


if __name__ == '__main__':
    unittest.main()
