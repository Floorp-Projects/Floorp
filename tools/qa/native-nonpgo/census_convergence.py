# SPDX-License-Identifier: MPL-2.0
"""Bounded read-only wait for the original strict census boundary to pass."""
import errno
import os
import shutil
import time

from census_diagnostics import identity
from checkpoint import measure_owned_boundary
from support import Failure,ProgressWatch,require


def safe_query(value):
    if not isinstance(value,dict):
        return {}
    return {key:value[key] for key in ('bytes','expectedBytes','errno')
            if type(value.get(key)) is int}


def safe_reason(value):
    allowed={'invalid-identity','bsd-zombie','invalid-bsd-state','permission','signal-zero-esrch',
             'short-bsd-info','short-task-info','generation-replaced','membership-changed',
             'invalid-task-counters','generation-changed-during-recheck'}
    return value if value is None or value in allowed else 'unknown-query-reason'


def read_diagnostic(value,row):
    """Fixed projections only; raw identities and arbitrary native text stay private."""
    status=value.get('status')
    result={'status':status if status in ('measured','gone','unavailable') else 'unavailable',
            'reason':safe_reason(value.get('reason')),'taskQuery':safe_query(value.get('taskQuery')),
            'sameCandidateBirth':value.get('_birth')==row[:3] if '_birth' in value else None}
    if 'bsdQuery' in value:result['fallbackBSDQuery']=safe_query(value['bsdQuery'])
    if 'fallbackReason' in value:
        reason=value['fallbackReason']
        result['fallbackReason']=reason if reason in ('measured','gone','unavailable') else safe_reason(reason)
    source=value.get('esrchRecheck')
    if isinstance(source,dict):
        trace={key:safe_query(source[key]) for key in
               ('firstTaskQuery','firstBSDQuery','taskQuery','bsdQuery') if key in source}
        if source.get('sameGeneration') is None or type(source.get('sameGeneration')) is bool:
            trace['sameGeneration']=source.get('sameGeneration')
        if source.get('outcome') in ('measured','gone','unavailable'):trace['outcome']=source['outcome']
        if 'reason' in source:trace['reason']=safe_reason(source['reason'])
        result['esrchRecheck']=trace
    return result


def candidate_read(row,reader,details):
    """At most two read() calls (each has at most two native task queries)."""
    def read():
        value=reader.read(row[0],expected={'pid':row[0],'startSec':row[1],'startUsec':row[2]})
        details.update(reason=safe_reason(value.get('reason')),taskQuery=safe_query(value.get('taskQuery')))
        details['readDiagnostics'].append(read_diagnostic(value,row))
        return value
    details['readDiagnostics']=[]
    value=read()
    query=value.get('taskQuery',{})
    retry=(row[0]!=os.getpid() and value.get('status')=='unavailable' and
           value.get('reason')=='short-task-info' and value.get('fallbackReason')=='measured' and
           value.get('_birth')==row[:3] and query.get('bytes')==0 and
           query.get('expectedBytes')==232 and query.get('errno')==errno.ESRCH)
    if retry:
        details.update(operation='candidate-bsd-recheck',reason=None)
        query={};state,anchor,reason=reader.bsd(row[0],query)
        matches=anchor is not None and identity(anchor)==row
        details.update(bsdQuery=safe_query(query),retryBSDQuery=safe_query(query),
                       reason=safe_reason(reason),retryBSDState=state if state in ('measured','gone','unavailable') else 'unavailable',
                       retryBSDReason=safe_reason(reason),retryIdentityMatches=matches)
        if state=='gone' and (matches or reason=='signal-zero-esrch'):
            return {'status':'gone','reason':reason,'_birth':row[:3],
                    '_goneConfirmed':True}
        require(state=='measured' and matches,'census-wait-identity-unavailable')
        details.update(operation='candidate-task-recheck',reason=None)
        value=read()
    if row[0]!=os.getpid() and value.get('status')=='gone' and (
            (value.get('reason')=='bsd-zombie' and value.get('_birth')==row[:3]) or
            value.get('reason')=='signal-zero-esrch'):
        # Bind credentials as well as birth after the internal task/BSD fallback.
        details.update(operation='candidate-bsd-confirm-gone',reason=None)
        query={};state,anchor,reason=reader.bsd(row[0],query)
        matches=anchor is not None and identity(anchor)==row
        details.update(bsdQuery=safe_query(query),goneBSDQuery=safe_query(query),
                       reason=safe_reason(reason),goneBSDState=state if state in ('measured','gone','unavailable') else 'unavailable',
                       goneBSDReason=safe_reason(reason),goneIdentityMatches=matches)
        require(state=='gone' and (matches or reason=='signal-zero-esrch'),
                'census-wait-identity-changed')
        return {**value,'_goneConfirmed':True}
    return value


def resources(rows,reader,details=None):
    """Upper bound self plus every new candidate; unknown is never zero."""
    details={} if details is None else details
    details.update(operation='self-bsd',reason=None)
    query={}
    own_state,own,reason=reader.bsd(os.getpid(),query)
    details.update(reason=safe_reason(reason),bsdQuery=safe_query(query))
    require(own_state=='measured' and own is not None,'census-wait-self-unavailable')
    all_rows=set(rows)|{identity(own)}
    rss,cpu,births,states,gone=0.,0.,[],set(),0
    candidate_rss,candidate_cpu,candidate_births,candidate_states=0.,0.,[],set()
    rechecks=[];recheck_candidates=0
    details.update(resourceRecheckCandidates=0,resourceRecheckDiagnostics=rechecks)
    for index,row in enumerate(sorted(all_rows)):
        details.pop('taskQuery',None)
        for key in ('readDiagnostics','retryBSDQuery','retryBSDState','retryBSDReason','retryIdentityMatches',
                    'goneBSDQuery','goneBSDState','goneBSDReason','goneIdentityMatches'):
            details.pop(key,None)
        details.update(operation='candidate-bsd-before',candidateIndex=index,reason=None,bsdQuery={})
        query={}
        state,before,reason=reader.bsd(row[0],query)
        details.update(reason=safe_reason(reason),bsdQuery=safe_query(query))
        if state=='gone' and row[0]!=os.getpid() and (
                (before is not None and identity(before)==row) or reason=='signal-zero-esrch'):
            gone+=1
            continue
        require(state=='measured' and before is not None and identity(before)==row,
                'census-wait-identity-unavailable')
        details.update(operation='candidate-task',reason=None)
        value=candidate_read(row,reader,details)
        reads=details['readDiagnostics']
        if len(reads)>1 or any('esrchRecheck' in item for item in reads):
            recheck_candidates+=1
            rechecks.append({key:item for key,item in details.items() if key in
                ('candidateIndex','readDiagnostics','retryBSDQuery','retryBSDState','retryBSDReason',
                 'retryIdentityMatches','goneBSDQuery','goneBSDState','goneBSDReason','goneIdentityMatches')})
            del rechecks[:-16]
            details['resourceRecheckCandidates']=recheck_candidates
        if row[0]!=os.getpid() and value.get('_goneConfirmed'):
            gone+=1
            continue
        require(value['status']=='measured' and value.get('_birth')==row[:3],
                'census-wait-resource-unavailable')
        details.update(operation='candidate-bsd-after')
        query={}
        state,after,reason=reader.bsd(row[0],query)
        details.update(reason=safe_reason(reason),bsdQuery=safe_query(query))
        require((state=='measured' and after is not None and identity(after)==row) or
                (row[0]!=os.getpid() and state=='gone' and
                 ((after is not None and identity(after)==row) or reason=='signal-zero-esrch')),
                'census-wait-identity-changed')
        rss+=value['rssMiB'];cpu+=value['cpuSeconds']
        births.append(row[:3]);states.add(value['state'])
        if row[0]!=os.getpid():
            candidate_rss+=value['rssMiB'];candidate_cpu+=value['cpuSeconds']
            candidate_births.append(row[:3]);candidate_states.add(value['state'])
    return {'status':'measured','sampleComplete':True,'rssMiB':rss,'cpuSeconds':cpu,
            'processes':len(births),'states':sorted(states),'_pids':tuple(births),
            'newCandidateIdentities':len(rows),'confirmedGoneCandidates':gone,
            'resourceRecheckCandidates':recheck_candidates,'resourceRecheckDiagnostics':rechecks,
            '_progress':{'rssMiB':candidate_rss,'cpuSeconds':candidate_cpu,
                         'processes':len(candidate_births),'states':sorted(candidate_states),
                         '_pids':tuple(candidate_births)},
            'scope':'self plus every new UID/RUID candidate; unrelated candidates are included'}


def metadata_resource_gate(rows,reader,details):
    """Fixed Failure codes survive the existing safe unittest receipt projection."""
    require(bool(rows),'census-wait-metadata-no-candidate')
    try:
        value=resources(rows,reader,details)
    except Exception as error:
        permission=(isinstance(error,PermissionError) or
                    (isinstance(error,OSError) and error.errno in (1,13)) or
                    details.get('reason')=='permission' or
                    details.get('taskQuery',{}).get('errno') in (1,13) or
                    details.get('bsdQuery',{}).get('errno') in (1,13))
        if permission:
            if details.get('operation') in ('candidate-task','candidate-task-recheck'):
                raise Failure('census-wait-metadata-task-permission') from None
            raise Failure('census-wait-metadata-identity-permission') from None
        raise Failure('census-wait-metadata-query-unavailable') from None
    require(value['_progress']['processes']>0,'census-wait-metadata-no-measurement')
    require(value['rssMiB']<=10240,'owned-rss-limit')
    return value


def wait_for_boundary(baseline,receipt,runner,*,deadline_minutes=260,interval_seconds=1,
                      reader=None,measure=None,sleep=time.sleep):
    require(0<deadline_minutes<runner.limits['jobMinutes'] and interval_seconds==1,
            'census-wait-budget')
    if reader is None:
        from darwin_activity import probe
        reader=probe()
    measure=measure or measure_owned_boundary
    receipt.update(status='waiting',baselineReset=False,candidatesExcluded=False,signalsSent=False,
                   deadlineMinutes=deadline_minutes,intervalSeconds=interval_seconds,samples=[],
                   firstBoundary=None,finalBoundary=None,peakRSSMiB=None,minimumFreeGiB=None,
                   deltaDetailsPolicy='first boundary and most recent 16 boundaries; all sample counts retained')
    watch=ProgressWatch(runner.limits['stallQuietMinutes'])
    try:
        while True:
            elapsed=runner.elapsed()
            require(elapsed<deadline_minutes*60 and elapsed<runner.limits['jobMinutes']*60,
                    'census-wait-phase-deadline')
            require(not runner.groups and runner.failure is None and runner.monitor_failure is None,
                    'census-wait-stage-cleanup')
            value={'elapsedSeconds':round(elapsed,3),'boundary':{},
                   'resource':{'status':'collecting','sampleComplete':False,'rssMiB':None,'cpuSeconds':None}}
            receipt['samples'].append(value)
            # This cap exceeds all possible samples within the unchanged 260min phase.
            require(len(receipt['samples'])<=15601,'census-wait-sample-limit')
            rows=measure(baseline,value['boundary'],runner.observed_processes)
            if receipt['firstBoundary'] is None:
                receipt['firstBoundary']=dict(value['boundary'])
            if len(receipt['samples'])>16:
                older=receipt['samples'][-17]['boundary']
                older.pop('deltaRecords',None)
                older['deltaDetailsRetained']=False
            try:
                activity=resources(rows,reader,value['resource'])
                value['resource']={key:item for key,item in activity.items() if not key.startswith('_')}
            except Exception as error:
                value['resource']['status']='unavailable'
                codes={'census-wait-self-unavailable','census-wait-identity-unavailable',
                       'census-wait-resource-unavailable','census-wait-identity-changed'}
                value['resource']['failureCode']=str(error) if isinstance(error,Failure) and str(error) in codes else 'resource-query-exception'
                if isinstance(error,OSError) and type(error.errno) is int:
                    value['resource']['errno']=error.errno
                runner.monitor_failure=runner.record_monitor('checkpoint-census-wait',value['resource'])
                raise Failure('monitor-unavailable') from None
            free=shutil.disk_usage(runner.task).free
            value['freeGiB']=round(free/1024**3,3)
            receipt['peakRSSMiB']=max(receipt['peakRSSMiB'] or 0,activity['rssMiB'])
            receipt['minimumFreeGiB']=min(receipt['minimumFreeGiB'] or value['freeGiB'],value['freeGiB'])
            require(activity['rssMiB']<=runner.limits['maxOwnedRSSMiB'],'owned-rss-limit')
            require(free>=runner.limits['minFreeDuringBuildGiB']*1024**3,'free-disk-reserve')
            log_bytes=0
            for record in runner.records:
                path=runner.task/(record['stage']+'.private.log')
                size=path.stat().st_size
                require(size<=runner.limits['maxStageLogBytes'],'private-log-limit')
                log_bytes+=size
            private_bytes=0
            private=runner.task/'native-private'
            for index,path in enumerate(private.rglob('*') if private.is_dir() else ()):
                require(index<200000,'native-private-entry-limit')
                if path.is_file() and not path.is_symlink():
                    private_bytes+=path.stat().st_size
                require(private_bytes<=runner.limits['maxNativePrivateBytes'],'native-private-size-limit')
            quiet,stalled=watch.observe(runner.elapsed(),log_bytes,free,activity['_progress'])
            value['quietSeconds']=round(quiet,3)
            require(not stalled,'multisignal-stall')
            require(runner.elapsed()<deadline_minutes*60,'census-wait-phase-deadline')
            if not rows:
                require(value['boundary']['status']=='passed' and
                        value['boundary']['newCandidateIdentities']==0,'census-wait-final-boundary')
                receipt.update(status='passed',finalBoundary=value['boundary'])
                return value['boundary']
            remaining=deadline_minutes*60-runner.elapsed()
            require(remaining>0,'census-wait-phase-deadline')
            sleep(min(interval_seconds,remaining))
    except BaseException:
        receipt['status']='failed'
        raise
