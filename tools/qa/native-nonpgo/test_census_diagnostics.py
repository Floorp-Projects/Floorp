# SPDX-License-Identifier: MPL-2.0
import json
import os
from types import SimpleNamespace
import unittest
from unittest import mock

import census_diagnostics as cd
from support import Runner


class CensusDiagnostics(unittest.TestCase):
    row = (90001, 123456789, 654321, 501, 501)

    def value(self, **changes):
        return SimpleNamespace(pid=self.row[0], start_sec=self.row[1], start_usec=self.row[2],
                               uid=self.row[3], ruid=self.row[4], ppid=os.getpid(), pgid=777,
                               name=b'python3.12', **changes)

    def reader(self, values):
        reader = mock.Mock()
        reader.bsd.side_effect = values
        return reader

    def test_verified_child_has_fixed_role_and_positive_stage_observation(self):
        value = self.value()
        reader = self.reader([('measured', value, None)]*2)
        hint = cd.describe(self.row, {self.row}, set(), {self.row[:3]}, reader)
        self.assertEqual(hint['status'], 'identity-verified')
        self.assertEqual(hint['role'], 'python')
        self.assertEqual(hint['parentRelation'], 'caller')
        self.assertEqual(hint['observedInStageGroup'], 'observed')
        self.assertFalse(hint['ancestryComplete'])
        self.assertFalse(hint['writerRoleEstablished'])

    def test_generation_or_credential_mismatch_never_binds_role(self):
        for field in ('start_sec','uid','ruid'):
            value = self.value(); setattr(value, field, getattr(value,field)+1)
            hint = cd.describe(self.row, {self.row}, set(), set(), self.reader([('measured',value,None)]))
            self.assertEqual(hint['status'], 'identity-changed-after-snapshot')
            self.assertIsNone(hint['role'])
            self.assertIsNone(hint['parentRelation'])

    def test_denial_exception_and_unknown_name_publish_no_free_text(self):
        for responses in ([('unavailable',None,'permission')], [OSError('PRIVATE-MESSAGE')]):
            hint = cd.describe(self.row, {self.row}, set(), set(), self.reader(responses))
            self.assertIn(hint['status'], ('unavailable','diagnostic-unavailable'))
            self.assertIsNone(hint['role'])
            self.assertNotIn('PRIVATE-MESSAGE', json.dumps(hint))
        value = self.value(); value.name=b'PRIVATE-EXECUTABLE'
        hint = cd.describe(self.row, {self.row}, set(), set(), self.reader([('measured',value,None)]*2))
        self.assertEqual(hint['role'], 'other')
        self.assertNotIn('PRIVATE-EXECUTABLE', json.dumps(hint))

    def test_parent_and_process_rechecks_must_match_birth_and_metadata(self):
        value = self.value(); value.ppid=90002
        parent = (90002,123456780,11,501,501)
        parent_value = self.value(); parent_value.pid=90002; parent_value.start_sec=123456780
        parent_value.start_usec=11
        reader = self.reader([('measured',value,None),('measured',parent_value,None),('measured',value,None)])
        hint=cd.describe(self.row,{self.row,parent},{parent},{parent[:3]},reader)
        self.assertEqual(hint['parentRelation'],'baseline-candidate')
        self.assertEqual(hint['parentObservedInStageGroup'],'observed')
        parent_value.start_sec+=1
        reader = self.reader([('measured',value,None),('measured',parent_value,None),('measured',value,None)])
        self.assertEqual(cd.describe(self.row,{self.row,parent},{parent},set(),reader)['parentRelation'],'unknown')
        later=self.value();later.pgid+=1
        hint=cd.describe(self.row,{self.row},set(),set(),self.reader([('measured',self.value(),None),('measured',later,None)]))
        self.assertEqual(hint['status'],'changed-during-diagnostic')
        self.assertIsNone(hint['role'])

    def test_group_numbers_alone_never_establish_stage_observation(self):
        value=self.value()
        hint=cd.describe(self.row,{self.row},set(),{(90001,123456788,654321)},self.reader([('measured',value,None)]*2))
        self.assertEqual(hint['observedInStageGroup'],'not-observed')

    def test_observer_is_bounded_and_unknown_cannot_authorize_cleanup(self):
        runner=object.__new__(Runner);runner.observed_processes=set();runner.observation_status='disabled'
        activity={'counterSource':'libproc-taskallinfo','sampleComplete':True,'_pids':(self.row[:3],)}
        runner.observe_processes(activity)
        self.assertEqual(runner.observed_processes,{self.row[:3]})
        self.assertEqual(runner.observation_status,'sampling')
        runner.observe_processes({**activity,'sampleComplete':False})
        self.assertEqual(runner.observation_status,'unavailable')
        runner.observe_processes(activity)
        self.assertEqual(runner.observation_status,'unavailable')
        runner.observed_processes={(pid,1,0) for pid in range(1,100001)}
        runner.observe_processes(activity)
        self.assertEqual(runner.observation_status,'truncated')
        self.assertEqual(len(runner.observed_processes),100000)
        runner.observe_processes(None)
        self.assertEqual(runner.observation_status,'unavailable')

