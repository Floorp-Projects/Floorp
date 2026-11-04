# SPDX-License-Identifier: MPL-2.0
"""Bound the short helper timing caller to one original Intel Mac job."""
import argparse
import contextlib
import datetime
import gc
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import time
import unittest
import urllib.error
import urllib.request
import warnings

from qa_core import Failure, require

REPOSITORY = 'Floorp-Projects/Floorp-Runtime'
BRANCH = 'codex/mac-native-helper-timing-recovery68'
JOB_NAME = 'helper-timing'
PROFILE = 'helper-timing-job-origin-v1'
API_CAP = 2 * 1024**2


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, newurl):
        return None


def _decode(raw):
    def pairs(rows):
        value = {}
        for key, item in rows:
            require(key not in value, 'fresh-origin-api-json')
            value[key] = item
        return value
    try:
        return json.loads(raw, object_pairs_hook=pairs,
            parse_constant=lambda _: require(False, 'fresh-origin-api-json'))
    except (ValueError, UnicodeError, RecursionError):
        raise Failure('fresh-origin-api-json') from None


def initialize(task, *, environment=None, opener=None, wall_clock=None, monotonic_clock=None):
    """No clock reset, checkpoint role, payload operation, or permission mutation."""
    env = os.environ if environment is None else environment
    wall = time.time if wall_clock is None else wall_clock
    mono = time.monotonic if monotonic_clock is None else monotonic_clock
    require(env.get('GITHUB_REPOSITORY') == REPOSITORY and env.get('GITHUB_EVENT_NAME') == 'push' and
            env.get('GITHUB_RUN_ATTEMPT') == '1' and env.get('GITHUB_REF') == 'refs/heads/' + BRANCH and
            env.get('FRESH_PUSH_CREATED') == 'true' and env.get('FRESH_PUSH_BEFORE') == '0' * 40,
            'fresh-origin-run-scope')
    run_id, head, token = env.get('GITHUB_RUN_ID'), env.get('GITHUB_SHA'), env.get('JOB_READ_TOKEN')
    require(type(run_id) is str and re.fullmatch(r'[1-9][0-9]{0,19}', run_id) is not None and
            type(head) is str and re.fullmatch(r'[0-9a-f]{40}', head) is not None and
            type(token) is str and 0 < len(token) <= 16384, 'fresh-origin-environment')
    start_mono = mono()
    require(type(start_mono) in (int, float) and math.isfinite(start_mono) and start_mono > 0,
            'fresh-origin-clock')
    deadline = start_mono + 90
    client = urllib.request.build_opener(NoRedirect) if opener is None else opener
    base = 'https://api.github.com/repos/' + REPOSITORY + '/actions/'
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}

    def within_api_deadline():
        now = mono()
        return type(now) in (int, float) and math.isfinite(now) and start_mono <= now < deadline

    def api(suffix):
        require(within_api_deadline(), 'fresh-origin-api-deadline')
        try:
            with client.open(urllib.request.Request(base + suffix, headers=headers), timeout=30) as response:
                require(type(response.status) is int and response.status == 200, 'fresh-origin-http')
                raw = response.read(API_CAP + 1)
        except (urllib.error.URLError, OSError):
            raise Failure('fresh-origin-http') from None
        require(type(raw) is bytes and len(raw) <= API_CAP, 'fresh-origin-api-size')
        require(within_api_deadline(), 'fresh-origin-api-deadline')
        return _decode(raw)

    run = api('runs/' + run_id)
    require(type(run) is dict and type(run.get('id')) is int and run['id'] == int(run_id) and
            run.get('head_sha') == head and run.get('head_branch') == BRANCH and
            type(run.get('run_attempt')) is int and run['run_attempt'] == 1 and run.get('event') == 'push' and run.get('status') == 'in_progress' and
            type(run.get('repository')) is dict and run['repository'].get('full_name') == REPOSITORY and
            run['repository'].get('private') is False,
            'fresh-origin-run-provenance')
    jobs = api('runs/' + run_id + '/jobs')
    require(type(jobs) is dict and type(jobs.get('total_count')) is int and jobs['total_count'] == 1 and
            type(jobs.get('jobs')) is list and len(jobs['jobs']) == 1, 'fresh-origin-job-count')
    job = jobs['jobs'][0]
    require(type(job) is dict and type(job.get('id')) is int and job['id'] > 0 and
            type(job.get('run_id')) is int and job['run_id'] == int(run_id) and
            type(job.get('run_attempt')) is int and job['run_attempt'] == 1 and job.get('head_sha') == head and
            job.get('name') == JOB_NAME and job.get('status') == 'in_progress' and
            job.get('labels') == ['macos-15-intel'] and type(job.get('started_at')) is str and
            re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', job['started_at']) is not None,
            'fresh-origin-current-job')
    try:
        started = datetime.datetime.fromisoformat(job['started_at'].replace('Z', '+00:00')).timestamp()
    except (ValueError, OverflowError):
        raise Failure('fresh-origin-started-at') from None
    now_wall, now_mono = wall(), mono()
    require(all(type(value) in (int, float) and math.isfinite(value) for value in
                (started, now_wall, now_mono)) and start_mono <= now_mono < deadline, 'fresh-origin-clock')
    age = now_wall - started
    origin = now_mono - max(0, age)
    require(-2 <= age < 600 and 0 < origin <= now_mono, 'fresh-origin-job-age')
    record = {'schema': 1, 'profile': PROFILE, 'repository': REPOSITORY, 'run': int(run_id),
              'attempt': 1, 'head': head, 'ref': env['GITHUB_REF'], 'event': 'push',
              'job': job['id'], 'jobName': JOB_NAME, 'startedAt': job['started_at'],
              'platformMinutes': 20, 'workDeadlineMinutes': 15, 'clockResetAllowed': False, 'monotonicOrigin': origin}
    supplied = Path(task)
    require(not supplied.is_symlink(), 'fresh-origin-task-exists-or-unavailable')
    task = supplied.resolve()
    try:
        task.mkdir(mode=0o700, exist_ok=False)
    except OSError:
        raise Failure('fresh-origin-task-exists-or-unavailable') from None
    metadata = task.lstat()
    require(stat.S_ISDIR(metadata.st_mode) and metadata.st_uid == os.geteuid() and
            stat.S_IMODE(metadata.st_mode) == 0o700, 'fresh-origin-task-binding')
    try:
        for name, raw in (('started', str(started).encode()), ('started-monotonic', str(origin).encode()),
                          ('job-origin.json', (json.dumps(record, sort_keys=True, indent=2) + '\n').encode())):
            descriptor = os.open(task / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            try:
                with os.fdopen(descriptor, 'wb') as output:
                    descriptor = None
                    output.write(raw)
            finally:
                if descriptor is not None:
                    os.close(descriptor)
    except OSError:
        # Leave an incomplete private task as failed evidence; never reset/reuse it.
        raise Failure('fresh-origin-write') from None
    return record


HERE=Path(__file__).resolve().parent
SELECTION_SHA256='390acf29b0559d39ae2320eb3399406282850a83d54e11f4a0390b358df6bdfb'
PHASES=('idle','contended')
VARIANTS=('lean','support-preimport')
STAGES=('census-abi-compile','census-abi','helper-timing-tests','helper-import-workload')
FAILURE_MARKERS=('first-failure.json','helper-timing-final-failed.json','helper-timing-postemit-failed.json')
ABI_PHASES=('abi-path','abi-files','abi-json','abi-schema','abi-binding-env','abi-origin-hash',
            'abi-source-hash','abi-inventory-hash','abi-binding-check','abi-binary-hash','abi-layout','abi-complete')


def integer(value,minimum=0,maximum=2**63-1):
    return type(value) is int and minimum<=value<=maximum


def number(value):
    return type(value) in (int,float) and math.isfinite(value) and 0<=value<2**63


def none_fields(value,*keys):
    return type(value) is dict and all(key in value and value[key] is None for key in keys)


def public_failure(value):
    if type(value) is dict and value.get('source')=='runner':
        from helper_import_preflight import runner_failure
        return runner_failure(value)
    phases=('source-binding','origin','abi','tests','experiment','complete','finalization','post-emit')
    codes=('timing-execution-failed','timing-final-proof-failed','timing-final-state-changed')
    return {'source':'caller','phase':value.get('phase') if type(value) is dict and value.get('phase') in phases else 'finalization',
            'code':value.get('code') if type(value) is dict and value.get('code') in codes else 'timing-final-proof-failed'}


def file_bytes(path,cap=4*1024**2,*,private=False):
    descriptor=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        before=os.fstat(descriptor)
        require(stat.S_ISREG(before.st_mode) and 0<before.st_size<=cap and
                (not private or (before.st_uid==os.geteuid() and before.st_nlink==1 and
                stat.S_IMODE(before.st_mode)==0o600)),'timing-receipt-file')
        with os.fdopen(descriptor,'rb',closefd=False) as stream:raw=stream.read(cap+1)
        after=os.fstat(descriptor)
        identity=lambda info:(info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
        require(identity(before)==identity(after) and len(raw)==before.st_size,'timing-receipt-changed')
        return raw
    finally:os.close(descriptor)


def owned_json(path,cap=4*1024**2):
    return _decode(file_bytes(path,cap,private=True))


def store(path,value):
    raw=(json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+'\n').encode()
    require(len(raw)<=4*1024**2,'timing-receipt-size')
    descriptor=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
    with os.fdopen(descriptor,'wb') as stream:stream.write(raw)


def digest(path):
    return hashlib.sha256(file_bytes(path)).hexdigest()


def verify_sources(environment=None):
    env=os.environ if environment is None else environment
    pins=_decode(env['HELPER_SOURCE_PINS_JSON'].encode())
    require(type(pins) is dict and 1<=len(pins)<=200,'timing-source-pins')
    actual={str(p.relative_to(HERE)) for p in HERE.rglob('*') if p.is_file() and
            '__pycache__' not in p.parts and p.suffix!='.pyc'}
    require(set(pins)==actual,'timing-source-coverage')
    for name,expected in pins.items():
        require(type(name) is str and not name.startswith('/') and '\\' not in name and
                all(part not in ('','..','.') for part in name.split('/')) and
                type(expected) is str and re.fullmatch('[0-9a-f]{64}',expected),'timing-source-pin')
        path=HERE/name
        require(not path.is_symlink() and path.resolve().is_relative_to(HERE) and
                path.is_file() and digest(path)==expected,'timing-source-binding')
    require(re.fullmatch('[0-9a-f]{40}',env.get('FLOORP_FIXTURE_SHA','')) is not None,
            'timing-fixture-binding')
    return hashlib.sha256(json.dumps(pins,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def clock(task,origin_hash,*,within_work=True,monotonic=None):
    mono=time.monotonic if monotonic is None else monotonic
    origin=owned_json(task/'job-origin.json')
    require(digest(task/'job-origin.json')==origin_hash and origin['profile']==PROFILE and
            origin['repository']==REPOSITORY and origin['attempt']==1 and type(origin['attempt']) is int and
            origin['head']==os.environ['GITHUB_SHA'] and origin['ref']==os.environ['GITHUB_REF']=='refs/heads/'+BRANCH and
            type(origin['run']) is int and str(origin['run'])==os.environ['GITHUB_RUN_ID'] and
            origin['jobName']==JOB_NAME and origin['platformMinutes']==20 and
            origin['workDeadlineMinutes']==15 and origin['clockResetAllowed'] is False,'timing-origin-binding')
    started=datetime.datetime.fromisoformat(origin['startedAt'].replace('Z','+00:00')).timestamp()
    bound=origin['monotonicOrigin']
    require(number(bound) and bound>0 and float(file_bytes(task/'started-monotonic',128,private=True))==bound and
            float(file_bytes(task/'started',128,private=True))==started,'timing-clock-binding')
    elapsed=mono()-bound
    require(number(elapsed) and elapsed<(900 if within_work else 1200),'timing-clock-deadline')
    return elapsed


def selection():
    path=HERE/'helper_timing_selection.json'
    raw=file_bytes(path)
    require(hashlib.sha256(raw).hexdigest()==SELECTION_SHA256,'timing-selection-binding')
    value=_decode(raw)
    require(type(value) is list and len(value)==103 and len(set(value))==103 and
            all(type(item) is str for item in value),'timing-selection')
    return value


def lifecycle_pass(value):
    return (type(value) is dict and set(value)=={'created','reaped','streamsClosed','retainedAnchors',
            'activePopen','unraisable','resourceWarnings','gcCollected'} and integer(value['created'],1) and
            integer(value['reaped']) and value['created']==value['reaped'] and
            value['streamsClosed'] is True and value['gcCollected'] is True and
            all(type(value[key]) is int and value[key]==0 for key in
                ('retainedAnchors','activePopen','unraisable','resourceWarnings')))


def preflight(task):
    # Load the actual class before any tests mock subprocess.Popen.
    import primary_anchor
    expected=selection();seen=[];unraisable=[]
    original_init=subprocess.Popen.__init__;original_hook=sys.unraisablehook
    def track(child,*args,**kwargs):
        try:return original_init(child,*args,**kwargs)
        finally:
            if integer(getattr(child,'pid',None),1) and getattr(child,'_child_created',False):seen.append(child)
    class Result(unittest.TextTestResult):
        def __init__(self,*args,**kwargs):super().__init__(*args,**kwargs);self.started=[];self.successful=[]
        def startTest(self,test):self.started.append(test.id());super().startTest(test)
        def addSuccess(self,test):self.successful.append(test.id());super().addSuccess(test)
    def flatten(suite):
        for item in suite:
            if isinstance(item,unittest.TestSuite):yield from flatten(item)
            else:yield item.id()
    value={'status':'failed','selectionSHA256':SELECTION_SHA256,'loaded':[],'started':[],'successful':[],
           'total':None,'failures':None,'errors':None,'skipped':None,'expectedFailures':None,
           'unexpectedSuccesses':None,'lifecycle':None}
    try:
        subprocess.Popen.__init__=track
        sys.unraisablehook=lambda event:unraisable.append(True)
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter('always',ResourceWarning)
            suite=unittest.defaultTestLoader.loadTestsFromNames(expected)
            value['loaded']=list(flatten(suite))
            require(value['loaded']==expected and not unittest.defaultTestLoader.errors,'timing-tests-loaded')
            result=unittest.TextTestRunner(verbosity=2,resultclass=Result).run(suite)
            value.update(started=result.started,successful=result.successful,total=result.testsRun,
                failures=len(result.failures),errors=len(result.errors),skipped=len(result.skipped),
                expectedFailures=len(result.expectedFailures),unexpectedSuccesses=len(result.unexpectedSuccesses))
            # Strong references prevent opportunistic destructor reaps. Unknown
            # handles are evidence of failure; this collector never signals them.
            gc.collect()
            value['lifecycle']={'created':len(seen),
                'reaped':sum(type(child.returncode) is int for child in seen),
                'streamsClosed':all(stream is None or stream.closed for child in seen
                                    for stream in (child.stdin,child.stdout,child.stderr)),
                'retainedAnchors':len(primary_anchor.retained_anchors()),
                'activePopen':len(subprocess._active),'unraisable':len(unraisable),
                'resourceWarnings':sum(issubclass(item.category,ResourceWarning) for item in caught),
                'gcCollected':True}
            validate_preflight(value,expected)
            value['status']='passed'
    finally:
        subprocess.Popen.__init__=original_init;sys.unraisablehook=original_hook
        store(task/'helper-timing-tests.json',value)
    return value


def validate_preflight(value,expected=None):
    expected=selection() if expected is None else expected
    require(type(value) is dict and value['selectionSHA256']==SELECTION_SHA256 and
            value['loaded']==value['started']==value['successful']==expected and
            type(value['total']) is int and value['total']==103 and
            all(type(value[key]) is int and value[key]==0 for key in
                ('failures','errors','skipped','expectedFailures','unexpectedSuccesses')) and
            lifecycle_pass(value['lifecycle']),'timing-tests-incomplete')


def valid_presence(value):
    return (type(value) is dict and set(value)=={'expectedWorkers','runningWorkers','complete','failure'} and
            type(value['expectedWorkers']) is int and value['expectedWorkers']==2 and
            type(value['runningWorkers']) is int and value['runningWorkers']==2 and
            value['complete'] is True and value['failure'] is None)


def validate_workload(value,layout):
    import helper_import_preflight as hp
    from darwin_inventory import timing_projection
    require(type(value) is dict and value['status']=='passed' and
            none_fields(value,'firstFailure','firstControlFailure') and
            not any(key in value for key in ('receiptWriteFailure','workloadReceiptFailure')),
            'timing-workload-failure')
    for key,expected in (('seconds',60),('pairsPerPhase',6),('workers',2),('workerMiB',128),
                         ('maximumDiskMiB',64),('readyWorkers',2),('barriersReleased',2),('samplingReserveSeconds',8)):
        require(type(value[key]) is int and value[key]==expected,'timing-workload-config')
    require(number(value['loadSeconds']) and value['loadSeconds']>=60 and
            value['pressureScope']=='bounded-small-memory-and-disk-control' and
            value['osCacheState']=='unknown-cold-or-warm' and all(value[key] is False for key in
                ('productionRSSReproduction','osCacheFlushed','exactOldProgramReproduction',
                 'startupRepairEstablished','originalCauseEstablished','rawProcessMetadataPublished')),
            'timing-workload-scope')
    expected=[(phase,variant,i) for phase in PHASES for i in range(1,7) for variant in VARIANTS]
    samples=value['samples'];checks=value['loadChecks']
    require(type(samples) is list and type(checks) is list and len(samples)==len(checks)==24,'timing-sample-count')
    layout_sha=hashlib.sha256(json.dumps(layout,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    safe=[]
    for row,check,identity in zip(samples,checks,expected):
        require(type(row) is dict and type(check) is dict and
                tuple(row[key] for key in ('phase','variant','ordinal'))==identity and type(row['ordinal']) is int and
                tuple(check[key] for key in ('phase','variant','ordinal'))==identity and type(check['ordinal']) is int,
                'timing-sample-identity')
        phase,variant,index=identity
        require(row['status']=='measured' and row['timeoutObserved'] is False and
                none_fields(row,'failure') and number(row['elapsedSeconds']) and
                type(row['cleanup']) is dict and set(row['cleanup'])=={'authority','reaped','killedOnTimeout','failure'} and
                row['cleanup']['authority']=='subprocess.run-exact-child' and row['cleanup']['reaped'] is True and
                row['cleanup']['killedOnTimeout'] is False and row['cleanup']['failure'] is None,'timing-sample-incomplete')
        times=timing_projection(row['progress'])
        require(times is not None and row['progress']['phase']=='group-decode' and
                [item['phase'] for item in times][:15]==['project-imports','imports-complete','abi-binding',*ABI_PHASES],
                'timing-sample-progress')
        measurement=row['measurement']
        require(type(measurement) is dict and set(measurement)=={'api','snapshotBytes','snapshotEntries',
                'selfBindingVerified','layoutSHA256'} and measurement['api']=='sysctl-kern-proc-pgrp' and
                measurement['selfBindingVerified'] is True and measurement['layoutSHA256']==layout_sha and
                integer(measurement['snapshotEntries'],1,32768) and
                integer(measurement['snapshotBytes'],1,8*1024**2) and
                measurement['snapshotBytes']==measurement['snapshotEntries']*layout['recordBytes'],
                'timing-sample-measurement')
        graph=hp.import_graph(json.dumps({'helperImportGraph':row['importGraph']}),variant)
        require(graph['modules']['support'] is (variant=='support-preimport') and
                all(graph['modules'][name] is True for name in ('qa_core','process_census','ctypes')),
                'timing-sample-graph')
        for presence in (row['loadBefore'],row['loadAfter'],check['before'],check['after']):
            require(presence is None if phase=='idle' else valid_presence(presence),'timing-load-presence')
        safe.append({'phase':phase,'variant':variant,'ordinal':index,'status':'measured',
            'elapsedSeconds':row['elapsedSeconds'],'progress':{'phase':'group-decode','events':len(times),'timings':times},
            'measurement':dict(measurement),'moduleCount':graph['moduleCount'],'exactHelperReaped':True})
    counts=value['counts']
    require(type(counts) is dict and set(counts)==set(PHASES),'timing-sample-counts')
    for phase in PHASES:
        require(type(counts[phase]) is dict and set(counts[phase])==set(VARIANTS),'timing-sample-counts')
        for variant in VARIANTS:
            row=counts[phase][variant]
            require(type(row) is dict and set(row)=={'samples','validMeasurements','timeouts'} and
                    all(type(row[k]) is int and row[k]==v for k,v in
                        (('samples',6),('validMeasurements',6),('timeouts',0))),'timing-sample-counts')
    workers=value['workerMeasurements'];cleanup=value['cleanup']
    require(type(workers) is list and type(cleanup) is list and len(workers)==len(cleanup)==2,'timing-worker-count')
    for index,(worker,reap) in enumerate(zip(workers,cleanup)):
        require(type(worker) is dict and type(reap) is dict and integer(worker['index']) and worker['index']==index and
                integer(reap['index']) and reap['index']==index and worker['status']=='passed' and
                worker['sufficient'] is True and worker['failurePresent'] is False and
                worker['mode']=='duration' and worker['iterations'] is None and worker['seconds']==60 and
                type(worker['seconds']) in (int,float) and type(worker['workerMiB']) is int and worker['workerMiB']==128 and
                type(worker['diskMiB']) is int and worker['diskMiB']==(64 if index==0 else 0) and
                type(worker['exitCode']) is int and worker['exitCode']==0 and reap['reaped'] is True and
                type(reap['exitCode']) is int and reap['exitCode']==0 and none_fields(reap,'failure') and
                ('streamCloseFailed' not in reap or reap['streamCloseFailed'] is False),'timing-worker-cleanup')
        for key in ('touchedBytes','cpuIterations','cpuHashedBytes','diskInitialWriteBytes',
                    'diskReadIterations','diskReadBytes','fsyncs'):
            require(integer(worker[key]),'timing-worker-measurement')
        require(worker['touchedBytes']==128*1024**2 and worker['cpuIterations']>0 and
                worker['cpuHashedBytes']==worker['cpuIterations']*4*1024**2 and
                number(worker['elapsedSeconds']) and worker['elapsedSeconds']>=60 and
                worker['diskInitialWriteBytes']==(64*1024**2 if index==0 else 0),'timing-worker-measurement')
        require((worker['fdClosed'] is True and worker['diskReadIterations']>0 and
                worker['diskReadBytes']==worker['diskReadIterations']*64*1024**2 and
                worker['fsyncs']==worker['diskReadIterations']+1) if index==0 else
                worker['diskReadBytes']==worker['diskReadIterations']==worker['fsyncs']==0,
                'timing-worker-io')
    require(type(value['temporaryDiskRemoval']) is dict and
            value['temporaryDiskRemoval']=={'status':'removed-after-exact-reap','failure':None},'timing-disk-cleanup')
    return safe


def valid_stage(row,name):
    from helper_import_preflight import primary_anchor_complete
    return (type(row) is dict and row.get('stage')==name and type(row.get('exitCode')) is int and
        row['exitCode']==0 and row.get('groupCleanup')=='verified-gone' and
        none_fields(row,'reason','failurePhase','cleanupFailureCode','cleanupInventoryFailure') and
        row.get('cleanupFailurePresent') is False and type(row.get('leaderReap')) is dict and
        row['leaderReap'].get('status')=='reaped' and type(row['leaderReap'].get('exitCode')) is int and
        row['leaderReap']['exitCode']==0 and primary_anchor_complete(row.get('primaryAnchor'),0))


def validate_experiment(value,layout):
    require(type(value) is dict and value['status']=='passed' and type(value['schema']) is int and value['schema']==1 and
            none_fields(value,'firstFailure','monitorFailure') and value['abiReused'] is True and
            type(value['helperTimeoutSeconds']) is int and value['helperTimeoutSeconds']==3 and
            not any(key in value for key in ('receiptWriteFailure','workloadReceiptFailure')) and
            type(value['newArtifactsStored']) is int and value['newArtifactsStored']==0 and
            all(value[key] is False for key in ('sdkUsed','floorpBuildPerformed','originalCauseEstablished',
                'startupRepairEstablished','exactOldProgramReproduction','productionRSSReproduction','osCacheFlushed',
                'productionRetryPolicyChanged','resourceLimitsChanged','allCandidateAccountingChanged')),
            'timing-experiment-failure')
    require(type(value['stages']) is list and len(value['stages'])==1 and
            valid_stage(value['stages'][0],'helper-import-workload'),'timing-experiment-stage')
    return validate_workload(value['workload'],layout)


def project_stage(row):
    from helper_import_preflight import stage_complete,stage_projection
    require(stage_complete(row) and row['stage'] in STAGES,'timing-stage-incomplete')
    value=stage_projection(row);value['stage']=row['stage']
    return value


def execute(task):
    result={'schema':1,'status':'failed','phase':'source-binding','firstFailure':None,'originSHA256':None,
            'sourcePinsSHA256':None,'preflightStages':[],'experiment':None,'selfProbeVerified':False}
    runner=None
    try:
        result['sourcePinsSHA256']=verify_sources()
        require(sys.platform=='darwin','timing-platform')
        result['phase']='origin';initialize(task)
        result['originSHA256']=digest(task/'job-origin.json')
        clock(task,result['originSHA256'])
        from support import Runner,clean_environment
        from helper_import_preflight import select_abi,run_experiment
        runner=Runner(task,float(file_bytes(task/'started',128,private=True)),_decode(file_bytes(HERE/'plan.json'))['limits'])
        runner.phase_deadline_minutes=15
        env=clean_environment(task);(task/'tmp').mkdir(mode=0o700)
        env['PYTHONDONTWRITEBYTECODE']='1';result['phase']='abi'
        require(select_abi(task,runner,env) is False,'timing-abi-not-fresh')
        from darwin_activity import probe
        own=probe().read(os.getpid())
        require(own['status']=='measured' and number(own['rssMiB']) and number(own['cpuSeconds']),
                'timing-self-unavailable')
        result['selfProbeVerified']=True
        clock(task,result['originSHA256']);result['phase']='tests'
        runner.run('helper-timing-tests',[sys.executable,str(HERE/'helper_timing_caller.py'),
                   '--preflight','--task',str(task)],HERE,env,360,phase_deadline_minutes=15)
        value=owned_json(task/'helper-timing-tests.json');validate_preflight(value)
        require(value['status']=='passed','timing-tests-incomplete')
        result['preflightStages']=[project_stage(row) for row in runner.records]
        require([row['stage'] for row in result['preflightStages']]==list(STAGES[:3]),'timing-preflight-stages')
        clock(task,result['originSHA256']);result['phase']='experiment'
        result['experiment']=run_experiment(task,{})
        from process_census import load_abi
        validate_experiment(result['experiment'],load_abi(task/'census-abi.json'))
        clock(task,result['originSHA256']);result['status']='passed';result['phase']='complete'
    except BaseException:
        if runner is not None and runner.failure is not None:
            from helper_import_preflight import runner_failure
            result['firstFailure']=runner_failure(runner.failure)
        experiment=result.get('experiment')
        if result['firstFailure'] is None and type(experiment) is dict and experiment.get('firstFailure') is not None:
            result['firstFailure']=public_failure(experiment['firstFailure'])
        if result['firstFailure'] is None:result['firstFailure']={'source':'caller','phase':result['phase'],'code':'timing-execution-failed'}
    finally:
        if task.is_dir():
            if runner is not None:
                from helper_import_preflight import monitor_projection,stage_projection
                experiment=result.get('experiment')
                monitor=runner.monitor_failure
                if monitor is None and type(experiment) is dict:monitor=experiment.get('monitorFailure')
                result['monitorFailure']=monitor_projection(monitor)
                result['monitorDiagnostics']=[monitor_projection(row) for row in runner.monitor_diagnostics[-16:]]
                result['rawStageEvidence']=[{**stage_projection(row),'stage':row['stage'] if row['stage'] in STAGES else None}
                                            for row in runner.records]
            store(task/'helper-timing-driver.json',result)
    return result


def final_proof(task,driver):
    import primary_anchor
    require(driver['status']=='passed' and none_fields(driver,'firstFailure','monitorFailure') and
            driver['selfProbeVerified'] is True and verify_sources()==driver['sourcePinsSHA256'],
            'timing-driver-failure')
    clock(task,driver['originSHA256'])
    require(not any(os.path.lexists(task/name) for name in FAILURE_MARKERS),'timing-persisted-failure')
    require(type(driver['preflightStages']) is list and len(driver['preflightStages'])==3 and
            all(valid_stage(row,name) for row,name in zip(driver['preflightStages'],STAGES[:3])),
            'timing-preflight-stages')
    tests=owned_json(task/'helper-timing-tests.json');validate_preflight(tests)
    require(tests['status']=='passed','timing-tests-incomplete')
    experiment=owned_json(task/'helper-import-preflight.json')
    require(experiment==driver['experiment'],'timing-experiment-readback')
    from process_census import load_abi
    samples=validate_experiment(experiment,load_abi(task/'census-abi.json'))
    require(primary_anchor.retained_anchors()==() and not subprocess._active,'timing-live-handles')
    return {'schema':1,'status':'passed','qualification':'bounded-helper-diagnostic-only',
        'terminalJobSuccessRequired':True,'stdoutAloneQualifies':False,'firstFailure':None,
        'tests':{'total':103,'passed':103,'skipped':0},'samples':samples,
        'workerMeasurements':[{key:row[key] for key in ('index','touchedBytes','cpuIterations','cpuHashedBytes',
            'diskInitialWriteBytes','diskReadIterations','diskReadBytes','fsyncs','elapsedSeconds')}
            for row in experiment['workload']['workerMeasurements']],
        'groupCleanup':'verified-gone','diskCleanup':'removed-after-exact-reap','originalCauseEstablished':False,
        'fullHostQualification':False,'nativeAuthentication':'NOT_TESTED','newArtifacts':0,
        'clock':{'elapsedSeconds':clock(task,driver['originSHA256']),'workMinutes':15,'platformMinutes':20}}


def experiment_diagnostics(value):
    """Failure evidence only; no projected field grants successful qualification."""
    from helper_import_preflight import cleanup_diagnostic_projection,monitor_projection,number as safe_number
    value=value if type(value) is dict else {}
    work=value.get('workload');work=work if type(work) is dict else {}
    samples=work.get('samples');samples=samples if type(samples) is list else []
    rows=[]
    statuses=('measured','timeout','inventory-unavailable','cleanup-unverified','nonzero-exit',
              'invalid-reply','spawn-or-setup-failed')
    for row in samples[:24]:
        if type(row) is not dict:continue
        cleanup=row.get('cleanup');cleanup=cleanup if type(cleanup) is dict else {}
        progress=cleanup_diagnostic_projection({'helperProgress':row.get('progress')}).get('helperProgress')
        rows.append({'phase':row.get('phase') if row.get('phase') in PHASES else None,
            'variant':row.get('variant') if row.get('variant') in VARIANTS else None,
            'ordinal':row.get('ordinal') if integer(row.get('ordinal'),1,6) else None,
            'status':row.get('status') if row.get('status') in statuses else 'unavailable',
            'elapsedSeconds':safe_number(row.get('elapsedSeconds')),'progress':progress,
            'timeoutObserved':row.get('timeoutObserved') if type(row.get('timeoutObserved')) is bool else None,
            'exactHelperReaped':cleanup.get('reaped') if type(cleanup.get('reaped')) is bool else None})
    control=work.get('firstControlFailure');safe_control=None
    if control is not None:
        control=control if type(control) is dict else {}
        safe_control={'code':'helper-import-sample-unavailable',
            'phase':control.get('phase') if control.get('phase') in PHASES else None,
            'variant':control.get('variant') if control.get('variant') in VARIANTS else None,
            'ordinal':control.get('ordinal') if integer(control.get('ordinal'),1,6) else None}
    monitor=value.get('monitorDiagnostics');monitor=monitor if type(monitor) is list else []
    return {'firstFailure':public_failure(value['firstFailure']) if value.get('firstFailure') is not None else None,
        'firstControlFailure':safe_control,'samples':rows,'monitorFailure':monitor_projection(value.get('monitorFailure')),
        'monitorDiagnostics':[monitor_projection(row) for row in monitor[-16:]]}


def terminal_guard(task,driver):
    """Last observed I/O, then a clock/handle gate with no more successful-path I/O."""
    import primary_anchor
    current=owned_json(task/'helper-timing-driver.json')
    require(current==driver and current['status']=='passed' and
            none_fields(current,'firstFailure','monitorFailure'),'timing-driver-failure')
    experiment=owned_json(task/'helper-import-preflight.json')
    require(experiment==current['experiment'] and experiment['status']=='passed' and
            none_fields(experiment,'firstFailure','monitorFailure') and
            none_fields(experiment.get('workload'),'firstFailure','firstControlFailure') and
            not any(key in experiment for key in ('receiptWriteFailure','workloadReceiptFailure')),
            'timing-experiment-failure')
    origin=owned_json(task/'job-origin.json')
    clock(task,current['originSHA256'])
    require(not any(os.path.lexists(task/name) for name in FAILURE_MARKERS),'timing-persisted-failure')
    # All filesystem observations above may take time. Recheck after the last
    # one, using the bound clock already validated against the original job.
    bound=origin['monotonicOrigin'];now=time.monotonic()
    require(number(now) and number(bound) and bound>0 and 0<=now-bound<900,'timing-clock-deadline')
    require(primary_anchor.retained_anchors()==() and not subprocess._active,'timing-live-handles')


def finalize(task,emit=print):
    value={'schema':1,'status':'failed','qualification':'bounded-helper-diagnostic-only',
           'terminalJobSuccessRequired':True,'stdoutAloneQualifies':False,'firstFailure':None}
    snapshots={};driver=None
    try:
        driver=owned_json(task/'helper-timing-driver.json')
        require(os.environ.get('DIAGNOSTIC_STEP_OUTCOME')=='success','timing-step-failed')
        value=final_proof(task,driver)
        names=('job-origin.json','started','started-monotonic','helper-timing-driver.json',
               'helper-timing-tests.json','helper-import-preflight.json')
        snapshots={name:digest(task/name) for name in names}
        store(task/'helper-timing-final.json',value)
        require(owned_json(task/'helper-timing-final.json')==value,'timing-final-readback')
        snapshots['helper-timing-final.json']=digest(task/'helper-timing-final.json')
        final_proof(task,driver)
    except BaseException:
        value['status']='failed'
        value['firstFailure']=public_failure(driver.get('firstFailure') if type(driver) is dict else None)
        try:
            observed=owned_json(task/'helper-import-preflight.json')
            value['experimentDiagnostics']=experiment_diagnostics(observed)
        except BaseException:pass
        try:store(task/'helper-timing-final-failed.json',value)
        except BaseException:pass
    raw=json.dumps({'helperTiming':value},allow_nan=False)
    require(len(raw.encode())<=1024**2,'timing-public-size')
    emit(raw,flush=True)
    if value['status']=='passed':
        try:
            final_proof(task,driver)
            require(all(digest(task/name)==expected for name,expected in snapshots.items()),'timing-post-emit-changed')
            terminal_guard(task,driver)
        except BaseException:
            value['status']='failed';value['firstFailure']={'source':'caller','phase':'post-emit','code':'timing-final-state-changed'}
            try:store(task/'helper-timing-postemit-failed.json',value)
            except BaseException:pass
            emit(json.dumps({'helperTiming':value},allow_nan=False),flush=True)
    return 0 if value['status']=='passed' else 1


def main():
    parser=argparse.ArgumentParser();modes=parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--execute',action='store_true');modes.add_argument('--preflight',action='store_true')
    modes.add_argument('--finalize',action='store_true');parser.add_argument('--task',type=Path,required=True)
    args=parser.parse_args()
    if args.execute:return 0 if execute(args.task)['status']=='passed' else 1
    if args.preflight:return 0 if preflight(args.task)['status']=='passed' else 1
    return finalize(args.task)


if __name__=='__main__':raise SystemExit(main())

