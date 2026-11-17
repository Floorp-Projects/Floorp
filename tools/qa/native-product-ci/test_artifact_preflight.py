# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import artifact_preflight as m

class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.pin={'schema':'floorp-debug-artifact-pin-v1','repository':m.RUNTIME,'profile':m.PROFILE,'runtime_sha':'a'*40,'artifact_sha256':'b'*64,'artifact_id':1,'run_id':2,'run_attempt':3,'workflow_id':4,'workflow_path':'.github/workflows/daily-build.yml','build_id':'20261117010101'}
        self.pin['product_profile']={'schema':'floorp-native-debug-product-profile-v1','productTestsExecuted':False}
        self.pin['artifact_name']=f"native-debug-linux-x86_64-v1-{'a'*40}-20261117010101-2-3"
        self.artifact={'id':1,'name':self.pin['artifact_name'],'expired':False,'digest':'sha256:'+'b'*64,'size_in_bytes':200,'workflow_run':{'id':2,'head_sha':'a'*40}}
        self.run={'id':2,'run_attempt':3,'workflow_id':4,'path':self.pin['workflow_path'],'head_sha':'a'*40,'head_branch':'nora-0.2.0','repository':{'full_name':m.RUNTIME},'event':'workflow_dispatch','status':'completed','conclusion':'success'}
        self.pr={'state':'open','head':{'sha':'c'*40,'repo':{'full_name':m.FLOORP,'fork':False}},'base':{'ref':'main','repo':{'full_name':m.FLOORP}}}
        self.host={'os':'ubuntu','version':'24.04','machine':'x86_64','pythonABI':'cp312','libc':['glibc','2.39']}
    def select(self):return m.validate_metadata(self.pin,self.artifact,self.run,self.pr,'c'*40)
    def test_valid_metadata(self):self.assertEqual(self.select()['attempt'],3)
    def test_invalid_metadata(self):
        cases=[('pin','runtime_sha','main'),('pin','artifact_name','latest'),('pin','workflow_path','.github/workflows/package.yml'),('artifact','expired',True),('artifact','digest','sha256:'+'f'*64),('artifact','id',9),('artifact','size_in_bytes',m.MAX_BYTES+1),('run','head_sha','d'*40),('run','run_attempt',1),('run','conclusion','failure'),('run','event','pull_request_target'),('run','head_branch','foreign'),('run','workflow_id',90),('pr','state','closed')]
        for target,key,value in cases:
            with self.subTest(target=target,key=key):
                obj=getattr(self,target);saved=copy.deepcopy(obj);obj[key]=value
                with self.assertRaises(ValueError):self.select()
                setattr(self,target,saved)
    def test_schedule_is_daily_workflow_only(self):
        self.run['event']='schedule';self.select()
        self.pin['workflow_path']=self.run['path']='.github/workflows/wrapper-build-linux.yml'
        with self.assertRaises(ValueError):self.select()
    def test_reject_fork_and_stale_pr(self):
        self.pr['head']['repo']['fork']=True
        with self.assertRaises(ValueError):self.select()
        self.pr['head']['repo']['fork']=False;self.pr['head']['sha']='d'*40
        with self.assertRaises(ValueError):self.select()
    def wrapper(self,path,change=None,extra=False):
        context=self.select()
        receipt={'schema':'qa3-hosted-debug-producer-v1','status':'PACKAGED_NOT_CONSUMED','context':context,'publicationAuthorized':False,'consumerVerdict':'UNVERIFIED','host':self.host,'productProfile':self.pin['product_profile'],'primarySha256':hashlib.sha256(b'primary').hexdigest(),'supportSha256':hashlib.sha256(b'support').hexdigest()}
        if change:change(receipt)
        with zipfile.ZipFile(path,'w') as z:
            z.writestr('runtime.tar.xz',b'primary');z.writestr('test-support.tar.gz',b'support');z.writestr('producer.json',json.dumps(receipt))
            if extra:z.writestr('../escape',b'no')
        self.pin['artifact_sha256']=m.digest(path)
        return context
    def test_transport_success_is_not_native_evidence(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bundle.zip';ctx=self.wrapper(p)
            result=m.inspect_wrapper(p,self.pin,ctx,self.host)
            self.assertEqual(result['status'],'TRANSPORT_ONLY_NOT_NATIVE_EVIDENCE')
            self.assertEqual(result['product_assembly'],'UNVERIFIED')
    def test_inner_receipt_and_host_rejections(self):
        mutations=[lambda r:r.update(productProfile={}),lambda r:r.update(primarySha256='f'*64),lambda r:r.update(consumerVerdict='PASS'),lambda r:r.update(publicationAuthorized=True),lambda r:r.update(context={}),lambda r:r.update(host={'os':'windows'})]
        for mutation in mutations:
            with self.subTest(mutation=mutation),tempfile.TemporaryDirectory() as d:
                p=Path(d)/'bundle.zip';ctx=self.wrapper(p,mutation)
                with self.assertRaises(ValueError):m.inspect_wrapper(p,self.pin,ctx,self.host)
                self.artifact['digest']='sha256:'+self.pin['artifact_sha256']
    def test_unexpected_wrapper_and_outer_digest(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bundle.zip';ctx=self.wrapper(p,extra=True)
            with self.assertRaises(ValueError):m.inspect_wrapper(p,self.pin,ctx,self.host)
            self.pin['artifact_sha256']='0'*64
            with self.assertRaises(ValueError):m.inspect_wrapper(p,self.pin,ctx,self.host)

if __name__=='__main__':unittest.main()
