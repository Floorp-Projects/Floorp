"""Host controls only. No actual logs, artifact bytes, executables, or API records."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import unittest
import zipfile
# Exact repo-relative module: the existing unittest entrypoint finds this
# test under tools/qa3 without changing workflows, PYTHONPATH or Deno config.
_CONTRACT = Path(__file__).resolve().parents[1] / 'qa/native-bridge/shadow17_contract.py'
_SPEC = importlib.util.spec_from_file_location('shadow17_contract', _CONTRACT)
_VERIFIER = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_VERIFIER)
check, PINNED_COMMITS, ROLES, SEALED_SCOPE = (
    _VERIFIER.check, _VERIFIER.PINNED_COMMITS, _VERIFIER.ROLES, _VERIFIER.SEALED_SCOPE
)

def digest(b):
    return hashlib.sha256(b).hexdigest()

def synthetic_archive(cases, fail_case=None):
    buf=io.BytesIO()
    rows=[dict(action='suite_start',tests={'browser.toml':list(cases)})]
    for case in cases:
        rows.extend([dict(action='test_start',test=case),
                     dict(action='test_status',test=case,subtest='host-only synthetic assertion',
                          status='FAIL' if case==fail_case else 'PASS'),
                     dict(action='test_end',test=case,status='PASS')])
    rows.append(dict(action='suite_end'))
    with zipfile.ZipFile(buf,'w') as z:
        z.writestr(zipfile.ZipInfo('raw.jsonl'),b''.join(json.dumps(r).encode()+b'\n' for r in rows))
    return buf.getvalue()

def stand_in(archive, tests, code):
    # Minimal unit-test collaborator, not an admission validator.
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        events=[json.loads(line) for line in z.read('raw.jsonl').splitlines()]
    if events[0]['tests']['browser.toml'] != tests:
        raise ValueError('selection mismatch')
    if any(r.get('status')=='FAIL' for r in events):
        raise ValueError('nonpassing synthetic assertion')
    return dict(state='SOURCE_CONTRACT_ONLY',raw_consistent=True,authenticated=False,
                native_pass=False,required_tests_observed=len(tests))

class Shadow17Tests(unittest.TestCase):
    slot_validator = staticmethod(stand_in)

    def setUp(self):
        self.p=dict(repository_id=1,run_id=2,attempt=3,job_id=4)
        self.calls=[]
        self.x=dict(producer=self.p,direct=dict(producer=dict(self.p),slots={}),policy={},
                    observations={},archives={},validate_slot=self.callback)
        for i,role in enumerate(ROLES):
            manifest,cases=SEALED_SCOPE[role]
            mat=dict(PINNED_COMMITS,manifest=manifest,profile=digest(role.encode()),
                     **{k:digest(k.encode()) for k in ('binary','overlay','support')})
            self.x['policy'][role]=dict(role=role,tests=cases,**mat)
            self.x['observations'][role]=dict(producer=dict(self.p),process_exit=0,**mat)
            self.archive(role,20+i,synthetic_archive(cases))

    def archive(self,role,aid,data):
        slot=dict(role=role,artifact_id=aid,artifact_digest=digest(data))
        self.x['direct']['slots'][role]=slot
        self.x['observations'][role].update(slot)
        self.x['archives'][role]=data

    def callback(self, archive, tests, code):
        self.calls.append(len(tests))
        return self.slot_validator(archive,tests,code)

    def deny(self, message):
        with self.assertRaisesRegex(ValueError,message):
            check(**self.x)

    def test_both_controls_remain_unqualified(self):
        out=check(**self.x)
        self.assertEqual(self.calls,[12,5])
        self.assertEqual(out['cohorts_checked'],ROLES)
        self.assertFalse(out['authenticated'] or out['native_pass'] or out['qualified'])

    def test_missing_slot(self):
        del self.x['direct']['slots']['History5']
        self.deny('shape')
        self.assertFalse(self.calls)

    def test_extra_slot(self):
        self.x['archives']['extra']=b'x'
        self.deny('shape')

    def test_direct_dependency_attempt_mismatch(self):
        self.x['direct']['producer']['attempt']=2
        self.deny('direct dependency producer/attempt mismatch')

    def test_cross_attempt_second_cohort(self):
        self.x['observations']['History5']['producer']['attempt']=2
        self.deny('observer producer/attempt mismatch')

    def test_substituted_policy_digest_on_both_sides(self):
        new=digest(b'not the real manifest')
        for source in ('observations','policy'):
            self.x[source]['History5']['manifest']=new
        self.deny('does not match exact role policy')

    def test_substituted_case_id(self):
        case_list=self.x['policy']['History5']['tests']
        self.x['policy']['History5']['tests']=('replacement.js',*case_list[1:])
        self.deny('does not match exact role policy')

    def test_subject_is_not_main_runtime_lock(self):
        for data in ('observations','policy'):
            for role in ROLES:
                self.x[data][role]['runtime']='741c4537e89b550b0297ed7ea493026506930d4d'
        self.deny('material identity mismatch: runtime')

    def test_swapped_bytes(self):
        self.x['archives']['State12'],self.x['archives']['History5']=(
            self.x['archives']['History5'],self.x['archives']['State12'])
        self.deny('archive bytes mismatch')

    def test_artifact_id_not_reused(self):
        self.archive('History5',20,self.x['archives']['History5'])
        self.deny('duplicate artifact')

    def test_same_bytes_not_reused(self):
        self.archive('History5',21,self.x['archives']['State12'])
        self.deny('duplicate artifact')

    def test_one_failure_even_with_zero_exit_and_pass_end(self):
        _,cases=SEALED_SCOPE['History5']
        self.archive('History5',21,synthetic_archive(cases,fail_case=cases[0]))
        self.deny('nonpassing')
        self.assertEqual(self.calls,[12,5])

    def test_nonzero_exit(self):
        self.x['observations']['History5']['process_exit']=1
        self.deny('exit zero')

    def test_shared_material_mismatch(self):
        for data in ('observations','policy'):
            self.x[data]['History5']['support']=digest(b'other files')
        self.deny('cross-cohort material mismatch')
