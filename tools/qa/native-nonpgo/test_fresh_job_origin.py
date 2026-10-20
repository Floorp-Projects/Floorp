# SPDX-License-Identifier: MPL-2.0
"""Portable normal-API models only: no HTTP, process, SDK or artifact operation."""
import copy
import datetime
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import urllib.error

import fresh_job_origin as origin
from qa_core import Failure


class Response(io.BytesIO):
    status = 200


class FreshJobOriginTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.task = self.root / 'task'
        self.env = {'GITHUB_REPOSITORY': origin.REPOSITORY, 'GITHUB_EVENT_NAME': 'push',
            'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_REF': 'refs/heads/' + origin.BRANCH,
            'FRESH_PUSH_CREATED': 'true', 'FRESH_PUSH_BEFORE': '0'*40,
            'GITHUB_RUN_ID': '12345', 'GITHUB_SHA': 'a'*40, 'JOB_READ_TOKEN': 'PRIVATE-SENTINEL'}
        self.run = {'id':12345, 'head_sha':'a'*40, 'head_branch':origin.BRANCH,
            'run_attempt':1, 'event':'push', 'status':'in_progress', 'repository':{'full_name':origin.REPOSITORY}}
        self.jobs = {'total_count':1, 'jobs':[{'id':23456, 'name':origin.JOB_NAME,
            'run_id':12345, 'run_attempt':1, 'head_sha':'a'*40, 'status':'in_progress', 'labels':['macos-15-intel'], 'started_at':'2026-10-20T12:00:00Z'}]}
        self.wall = datetime.datetime(2026,10,20,12,1,tzinfo=datetime.timezone.utc).timestamp()
        self.mono = 1000.0
        self.requests=[];self.responses=[]
        self.client=mock.Mock()
        self.client.open.side_effect=self.open

    def open(self, request, timeout):
        self.requests.append((request,timeout))
        response=Response(json.dumps(self.run if len(self.requests)==1 else self.jobs).encode())
        self.responses.append(response)
        return response

    def invoke(self):
        return origin.initialize(self.task,environment=self.env,opener=self.client,
            wall_clock=lambda:self.wall,monotonic_clock=lambda:self.mono)

    def test_normal_lineage_creates_original_clock_once_and_private_task(self):
        record=self.invoke()
        self.assertEqual(self.task.stat().st_mode & 0o777,0o700)
        self.assertEqual(float((self.task/'started-monotonic').read_text()),940)
        self.assertEqual(float((self.task/'started').read_text()),self.wall-60)
        self.assertEqual(json.loads((self.task/'job-origin.json').read_text()),record)
        self.assertEqual(record['jobName'],'fresh-firefox45')
        self.assertTrue(all(path.stat().st_mode & 0o777 == 0o600 for path in self.task.iterdir()))
        self.assertEqual((record['platformMinutes'],record['workDeadlineMinutes']),(360,345))
        self.assertIs(record['clockResetAllowed'],False)
        self.assertNotIn('role',record);self.assertNotIn('PRIVATE-SENTINEL',json.dumps(record))
        self.assertTrue(all(response.closed for response in self.responses))
        self.assertEqual([timeout for _,timeout in self.requests],[30,30])
        self.assertTrue(all(request.full_url.startswith('https://api.github.com/') for request,_ in self.requests))

    def test_existing_task_and_clock_are_not_reused_or_overwritten(self):
        self.task.mkdir();marker=self.task/'started-monotonic';marker.write_text('old')
        with self.assertRaisesRegex(Failure,'^fresh-origin-task-exists-or-unavailable$'):self.invoke()
        self.assertEqual(marker.read_text(),'old')

    def test_symlink_to_existing_task_cannot_reset_its_clock(self):
        actual=self.root/'actual';actual.mkdir();(actual/'started').write_text('old')
        self.task.symlink_to(actual)
        with self.assertRaises(Failure):self.invoke()
        self.assertEqual((actual/'started').read_text(),'old')

    def test_all_created_push_environment_guards_fail_before_api(self):
        for key,value in (('GITHUB_REPOSITORY','foreign'),('GITHUB_EVENT_NAME','workflow_dispatch'),
            ('GITHUB_RUN_ATTEMPT','2'),('GITHUB_REF','refs/heads/main'),('FRESH_PUSH_CREATED','false'),
            ('FRESH_PUSH_BEFORE','a'*40),('GITHUB_RUN_ID','../123'),('GITHUB_SHA','x'*40),('JOB_READ_TOKEN','')):
            old=self.env[key];self.env[key]=value
            with self.subTest(key=key),self.assertRaises(Failure):self.invoke()
            self.env[key]=old
        self.assertEqual(self.requests,[]);self.assertFalse(self.task.exists())

    def test_actual_run_fields_reject_wrong_types_or_origin(self):
        for key,value in (('id',True),('id',12346),('head_sha','b'*40),('head_branch','main'),
            ('run_attempt',True),('run_attempt',2),('event','pull_request'),('status','completed'),('repository',{'full_name':'foreign'})):
            self.requests.clear();old=self.run[key];self.run[key]=value
            with self.subTest(key=key),self.assertRaisesRegex(Failure,'run-provenance'):self.invoke()
            self.run[key]=old
        self.assertFalse(self.task.exists())

    def test_exactly_one_job_required_not_bool_or_pagination_omission(self):
        for value in ({'total_count':True,'jobs':self.jobs['jobs']},
            {'total_count':2,'jobs':self.jobs['jobs']}, {'total_count':1,'jobs':[]},
            {'total_count':1,'jobs':self.jobs['jobs']*2}):
            self.requests.clear();self.jobs=value
            with self.assertRaisesRegex(Failure,'job-count'):self.invoke()
        self.assertFalse(self.task.exists())

    def test_job_name_state_labels_id_and_timestamp_are_bound(self):
        row=self.jobs['jobs'][0]
        for key,value in (('id',True),('id',0),('name','checkpoint-consumer'),('status','completed'),
            ('labels',['self-hosted']),('started_at','PRIVATE-SENTINEL'),('started_at','2026-99-99T00:00:00Z')):
            self.requests.clear();old=row[key];row[key]=value
            with self.subTest(key=key),self.assertRaises(Failure):self.invoke()
            row[key]=old
        self.assertFalse(self.task.exists())

    def test_job_age_unknown_or_outside_original_600_seconds_is_rejected(self):
        initial=self.wall
        for wall in (initial+540,initial-63,float('nan'),True):
            self.requests.clear();self.wall=wall
            with self.subTest(wallType=type(wall).__name__),self.assertRaises(Failure):self.invoke()
        self.assertFalse(self.task.exists())

    def test_api_deadline_before_first_request_or_after_response_is_not_reset(self):
        for sequence in ((1000,1090),(1000,1001,1090)):
            with self.subTest(sequence=sequence),self.assertRaisesRegex(Failure,'api-deadline'):
                origin.initialize(self.task,environment=self.env,opener=self.client,
                    wall_clock=lambda:self.wall,monotonic_clock=mock.Mock(side_effect=sequence))
        self.assertFalse(self.task.exists())

    def test_http_errors_stop_without_task_or_secret_text(self):
        for error in (urllib.error.HTTPError('PRIVATE-SENTINEL',403,'PRIVATE-SENTINEL',{},None),
                      urllib.error.URLError('PRIVATE-SENTINEL')):
            self.client.open.side_effect=error
            with self.assertRaises(Failure) as raised:self.invoke()
            self.assertEqual(str(raised.exception),'fresh-origin-http')
        self.assertFalse(self.task.exists())

    def test_redirect_and_non200_are_stopped(self):
        response=Response(b'{}');response.status=302;self.client.open.side_effect=lambda *_a,**_k:response
        with self.assertRaisesRegex(Failure,'fresh-origin-http'):self.invoke()
        self.assertTrue(response.closed)
        self.assertIsNone(origin.NoRedirect().redirect_request(None,None,302,None,None,'PRIVATE-SENTINEL'))

    def test_oversized_duplicate_nonfinite_and_malformed_api_fail_closed(self):
        for raw in (b'x'*(origin.API_CAP+1),b'{"id":1,"id":2}',b'{"value":NaN}',b'PRIVATE-SENTINEL'):
            response=Response(raw);self.client.open.side_effect=lambda *_a,**_k:response
            with self.assertRaises(Failure):self.invoke()
            self.assertTrue(response.closed)
        self.assertFalse(self.task.exists())

    def test_partial_write_stops_and_task_is_not_reinitialized(self):
        original=os.open
        def fail(path,*args,**kwargs):
            if Path(path).name=='started-monotonic':raise PermissionError(13,'PRIVATE-SENTINEL')
            return original(path,*args,**kwargs)
        with mock.patch.object(origin.os,'open',fail),self.assertRaisesRegex(Failure,'fresh-origin-write'):self.invoke()
        self.requests.clear()
        with self.assertRaisesRegex(Failure,'task-exists-or-unavailable'):self.invoke()
        self.assertFalse((self.task/'job-origin.json').exists())

    def test_dangling_task_symlink_is_not_followed_or_created(self):
        target=self.root/'absent-target';self.task.symlink_to(target)
        with self.assertRaisesRegex(Failure,'task-exists-or-unavailable'):self.invoke()
        self.assertFalse(target.exists())

    def test_unknown_or_backwards_api_clock_does_not_issue_request(self):
        for now in (False,float('nan'),999.0):
            self.requests.clear()
            with self.assertRaisesRegex(Failure,'api-deadline'):
                origin.initialize(self.task,environment=self.env,opener=self.client,
                    wall_clock=lambda:self.wall,monotonic_clock=mock.Mock(side_effect=(1000,now)))
            self.assertEqual(self.requests,[])

    def test_api_http_status_is_typed(self):
        response=Response(b'{}');response.status=200.0
        self.client.open.side_effect=lambda *_a,**_k:response
        with self.assertRaisesRegex(Failure,'fresh-origin-http'):self.invoke()
        self.assertTrue(response.closed)

    def test_job_crossrun_attempt_and_head_are_rejected_with_actual_response_fields(self):
        row=self.jobs['jobs'][0]
        for key,value in (('run_id',12346),('run_id',True),('run_attempt',True),
                          ('run_attempt',2),('head_sha','b'*40)):
            self.requests.clear();old=row[key];row[key]=value
            with self.subTest(key=key),self.assertRaisesRegex(Failure,'current-job'):self.invoke()
            row[key]=old
        self.assertFalse(self.task.exists())

    def test_deep_json_is_a_fixed_failure_not_recursion_text(self):
        self.client.open.side_effect=lambda *_a,**_k:Response(b'['*100000+b'0'+b']'*100000)
        with self.assertRaisesRegex(Failure,'^fresh-origin-api-json$'):self.invoke()
        self.assertFalse(self.task.exists())

    def test_cli_unknown_failure_does_not_expose_path_or_exception_text(self):
        with mock.patch.object(origin,'initialize',side_effect=OSError(13,'PRIVATE-SENTINEL')),\
             mock.patch('sys.argv',['fresh_job_origin.py','--task',str(self.task)]),\
             mock.patch('sys.stdout',new_callable=io.StringIO) as output:
            self.assertEqual(origin.main(),1)
            self.assertEqual(json.loads(output.getvalue()),{'freshJobOrigin':'failed','code':'fresh-origin-unavailable'})

    def test_cli_failure_uses_fixed_code_without_url_or_token(self):
        with mock.patch.object(origin,'initialize',side_effect=Failure('fresh-origin-http')),\
             mock.patch('sys.argv',['fresh_job_origin.py','--task',str(self.task)]),\
             mock.patch('sys.stdout',new_callable=io.StringIO) as output:
            self.assertEqual(origin.main(),1)
            self.assertEqual(json.loads(output.getvalue()),{'freshJobOrigin':'failed','code':'fresh-origin-http'})


if __name__=='__main__':unittest.main()
