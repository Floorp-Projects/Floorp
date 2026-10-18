# SPDX-License-Identifier: MPL-2.0
import copy
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

import process_census as pc
from support import Failure


def layout():
    offsets={'pid':0,'state':4,'startSec':8,'startUsec':16,'uid':20,'ruid':24}
    return {'schema':1,'pointerBytes':8,'recordBytes':256,'byteOrder':'little','mib':[1,14,0,0],
            'fields':{name:{'offset':offsets[name],'bytes':size or 4,'signed':signed}
                      for name,(size,signed) in pc.FIELDS.items()}}


def raw(*rows):
    result=bytearray()
    for changes in rows:
        row={'pid':41,'state':2,'startSec':123,'startUsec':4,'uid':501,'ruid':501,**changes}
        data=bytearray(256)
        for name,field in layout()['fields'].items():
            data[field['offset']:field['offset']+field['bytes']]=row[name].to_bytes(field['bytes'],'little',signed=field['signed'])
        result+=data
    return bytes(result)


class Query:
    def __init__(self,responses):
        self.responses=list(responses)
        self.calls=[]

    def __call__(self,mib,count,output,length,newp,newlen):
        assert tuple(mib)==(1,14,0,0) and count==4 and newp is None and newlen==0
        self.calls.append('size' if output is None else 'data')
        response=self.responses.pop(0)
        size=ctypes.cast(length,ctypes.POINTER(ctypes.c_size_t))
        if output is not None and response.get('data'):
            assert len(response['data'])<=size[0]
            ctypes.memmove(output,response['data'],len(response['data']))
        size[0]=response.get('size',len(response.get('data',b'')))
        ctypes.set_errno(response.get('errno',0))
        return response.get('rc',0)


class CensusGuards(unittest.TestCase):
    def invoke(self,responses,own=None):
        query=Query(responses)
        system=SimpleNamespace(sysctl=query)
        probe=mock.Mock()
        probe.bsd.return_value=own or ('measured',SimpleNamespace(pid=41,uid=501,ruid=501,start_sec=123,start_usec=4),None)
        with mock.patch.object(pc.os,'getpid',return_value=41), \
                mock.patch.object(pc.os,'geteuid',return_value=501), \
                mock.patch.object(pc.os,'getuid',return_value=501):
            value=pc.snapshot(layout(),system,probe)
        return value,query

    def test_primary_snapshot_retains_mixed_candidates_and_excludes_only_valid_zombies(self):
        data=raw({}, {'pid':42,'uid':0}, {'pid':43,'ruid':0}, {'pid':44,'uid':0,'ruid':0}, {'pid':45,'state':5})
        value,query=self.invoke([{'size':len(data)},{'data':data}])
        self.assertEqual(value['identities'],{(41,123,4,501,501),(42,123,4,0,501),(43,123,4,501,0)})
        self.assertEqual(value['measurement']['candidateZombies'],1)
        self.assertEqual(value['measurement']['mixedCandidateIdentities'],2)
        self.assertEqual(query.calls,['size','data'])
        self.assertNotIn('identities',value['measurement'])

    def test_enomem_partial_is_discarded_before_fresh_size_and_data_queries(self):
        data=raw({})
        value,query=self.invoke([{'size':256},{'data':raw({'pid':666}),'errno':errno.ENOMEM,'rc':-1},
                                 {'size':256},{'data':data}])
        self.assertEqual(value['identities'],{(41,123,4,501,501)})
        self.assertEqual(value['measurement']['retryErrnos'],[errno.ENOMEM])
        self.assertEqual(query.calls,['size','data','size','data'])

    def test_permission_and_other_errors_never_fall_back_or_become_empty(self):
        for responses,code in (([{'rc':-1,'errno':errno.EPERM}],'census-sysctl-permission'),
                               ([{'size':256},{'rc':-1,'errno':errno.EACCES}],'census-sysctl-permission'),
                               ([{'rc':-1,'errno':errno.EIO}],'census-sysctl-size-unavailable')):
            with self.subTest(code=code),self.assertRaisesRegex(Failure,code):
                self.invoke(responses)

    def test_enomem_retries_are_bounded_and_never_parse_partial_rows(self):
        responses=[item for _ in range(3) for item in ({'size':256},{'data':raw({'pid':666}),'rc':-1,'errno':errno.ENOMEM})]
        with self.assertRaisesRegex(Failure,'census-sysctl-data-unavailable'):
            self.invoke(responses)

    def test_invalid_sizes_and_partial_record_are_unknown(self):
        for responses in ([{'size':0}],[{'size':pc.CAP+1}],[{'size':257}],
                          [{'size':256},{'data':raw({})[:-1]}],
                          [{'size':256},{'size':pc.CAP+1}]):
            with self.subTest(responses=len(responses)),self.assertRaises(Failure):
                self.invoke(responses)

    def test_duplicate_and_negative_pids_are_rejected(self):
        for data in (raw({},{'uid':0}),raw({'pid':-1})):
            with self.assertRaisesRegex(Failure,'census-snapshot-pid'):
                pc.decode(data,layout(),501,501,41,(123,4))

    def test_candidate_birth_state_and_usec_must_be_valid(self):
        for change in ({'startSec':0},{'startUsec':1000000},{'startUsec':-1},{'state':0},{'state':6}):
            with self.subTest(change=change),self.assertRaisesRegex(Failure,'census-snapshot-identity'):
                pc.decode(raw({}, {'pid':42,**change}),layout(),501,501,41,(123,4))
        for change in ({'uid':2**32-1},{'ruid':2**32-1}):
            with self.subTest(change=change),self.assertRaisesRegex(Failure,'census-snapshot-credentials'):
                pc.decode(raw({}, {'pid':42,**change}),layout(),501,501,41,(123,4))

    def test_self_identity_is_required_and_bound_to_birth_and_credentials(self):
        for data in (raw({'pid':42}),raw({'startSec':124}),raw({'ruid':0}),raw({'state':5})):
            with self.assertRaisesRegex(Failure,'census-snapshot-self-binding'):
                pc.decode(data,layout(),501,501,41,(123,4))

    def test_self_libproc_unavailable_is_not_ignored(self):
        with self.assertRaisesRegex(Failure,'census-self-unavailable'):
            self.invoke([],('unavailable',None,'permission'))

    def test_baseline_tracks_new_generation_and_changed_candidate_credentials(self):
        before=pc.decode(raw({}, {'pid':42,'uid':0}),layout(),501,501,41,(123,4))['identities']
        self.assertFalse(pc.decode(raw({}, {'pid':42,'uid':0}),layout(),501,501,41,(123,4))['identities']-before)
        for change in ({'pid':43,'uid':0},{'pid':42,'uid':0,'startSec':124},{'pid':42,'uid':501}):
            after=pc.decode(raw({},change),layout(),501,501,41,(123,4))['identities']
            self.assertEqual(len(after-before),1)

    def test_abi_rejects_out_of_bounds_overlapping_wrong_width_or_signedness(self):
        variants=[]
        for field,changes in (('uid',{'signed':True}),('startSec',{'bytes':4}),('ruid',{'offset':20}),('pid',{'offset':10000})):
            value=copy.deepcopy(layout());value['fields'][field].update(changes);variants.append(value)
        value=layout();value['byteOrder']='big';variants.append(value)
        for value in variants:
            with self.assertRaises(Failure):pc.validate_layout(value)

    def test_abi_receipt_is_bound_to_source_binary_job_and_fixture(self):
        with tempfile.TemporaryDirectory() as temporary:
            task=Path(temporary).resolve();(task/'job-origin.json').write_text('{"job":1}\n')
            binary=task/'census-abi';binary.write_bytes(b'fixture inspector');binary.chmod(0o500)
            env={key:'fixture-'+key for key in pc.BINDING_KEYS};env['TASK_ROOT']=str(task)
            with mock.patch.dict(os.environ,env):
                value={'schema':1,'binding':pc.binding(task),'binarySHA256':pc.digest(binary),'layout':layout(),
                       'layoutSHA256':hashlib.sha256(pc.canonical(layout())).hexdigest()}
                path=task/'census-abi.json';path.write_text(json.dumps(value));path.chmod(0o400)
                self.assertEqual(pc.load_abi(path),layout())
                with mock.patch.dict(os.environ,{'GITHUB_RUN_ID':'another'}),self.assertRaisesRegex(Failure,'census-abi-binding'):
                    pc.load_abi(path)
                binary.chmod(0o700);binary.write_bytes(b'tampered');binary.chmod(0o500)
                with self.assertRaisesRegex(Failure,'census-abi-binding'):pc.load_abi(path)
