# SPDX-License-Identifier: MPL-2.0
"""Observe a real native candidate selection without changing its API results."""
import errno
from unittest import mock

from monitor_ps_lifetime import child_status
from support import require

BOOLS=('readyAcknowledged','candidateListed','targetFiltered','targetBirthMatched','targetPathMatched',
       'targetArgvMatched','filteredBirthMatched','filteredArgvMatched','exactRowIncluded',
       'leaderReaped','groupGone','streamCloseFailed','selectionFailureTargetsChild')
COUNTS=('candidateCount','filteredCount','targetBSDReads','targetGoneReads','targetPathReads','targetArgvReads')
PHASES=('launch','readiness','initial-row','selection','predicates','release','zombie','cleanup','finished')


def fresh():
    return {'schema':1,'status':'failed','phase':'launch',
            **{key:None for key in BOOLS},**{key:None if key in ('candidateCount','filteredCount') else 0
                                           for key in COUNTS},
            'beforeState':None,'afterState':None,'beforeErrno':None,'afterErrno':None,
            'selectionAPI':None,'selectionErrorKind':None,'selectionErrno':None,'cleanupFailureKind':None}


def public(value):
    require(isinstance(value,dict) and set(value)==set(fresh()) and type(value['schema']) is int and
            value['schema']==1 and value['status'] in ('passed','failed') and value['phase'] in PHASES,
            'native-selection-diagnostic-shape')
    require(all(value[k] is None or type(value[k]) is bool for k in BOOLS) and
            all((value[k] is None and k in ('candidateCount','filteredCount')) or
                (type(value[k]) is int and 0<=value[k]<=100000) for k in COUNTS),
            'native-selection-diagnostic-fields')
    require(all(value[k] in (None,'live','exit-pending','unavailable') for k in ('beforeState','afterState')) and
            all(value[k] is None or type(value[k]) is int and 0<=value[k]<65536
                for k in ('beforeErrno','afterErrno','selectionErrno')) and
            value['selectionAPI'] in (None,'pids','bsd','path','argv','rows') and
            value['selectionErrorKind'] in (None,'permission','os-error','runtime-error','exception') and
            value['cleanupFailureKind'] in (None,'permission','timeout','process-lookup','os-error','exception',
                                           'group-still-present'),
            'native-selection-diagnostic-kind')
    return dict(value)


def lifetime(pid,value,prefix):
    try:
        state=child_status(pid)
        require(state['matchingChild'] is True and state['errno']==0 and
                type(state['exitPending']) is bool,'native-selection-child-status')
        value[prefix+'State']='exit-pending' if state['exitPending'] else 'live'
        value[prefix+'Errno']=0
    except Exception as error:
        value[prefix+'State']='unavailable'
        value[prefix+'Errno']=error.errno if isinstance(error,OSError) and type(error.errno) is int else None


def finish(child,value):
    from census_experiment import cleanup_child
    value['streamCloseFailed']=False
    for stream in (child.stdin,child.stdout):
        try:
            if stream is not None:stream.close()
        except Exception:value['streamCloseFailed']=True
    cleanup=cleanup_child(child)
    value.update(leaderReaped=cleanup['leaderReaped'],groupGone=cleanup['groupCleanup']=='verified-gone',
                 cleanupFailureKind=(cleanup['cleanupFailure'] or {}).get('kind'))
    passed=(value['leaderReaped'] and value['groupGone'] and value['cleanupFailureKind'] is None and
            not value['streamCloseFailed'])
    if not passed:value.update(status='failed',phase='cleanup')
    return passed


def select(inventory,expected,executable,value):
    """All observations come from this one rows() call; no retry or extra snapshot."""
    pid,birth,args=expected
    original={name:getattr(inventory,name) for name in ('pids','bsd','path','argv')}
    def error(api,failure,target=None):
        value.update(selectionAPI=api,selectionErrorKind='permission' if isinstance(failure,PermissionError)
                     else 'os-error' if isinstance(failure,OSError) else
                     'runtime-error' if isinstance(failure,RuntimeError) else 'exception',
                     selectionErrno=failure.errno if isinstance(failure,OSError) and
                         type(failure.errno) is int else None,selectionFailureTargetsChild=target)
    def update_bool(key,matched):
        value[key]=matched if value[key] is None else value[key] and matched
    def wrap(name):
        def call(*arguments,**keywords):
            try:result=original[name](*arguments,**keywords)
            except Exception as failure:
                error(name,failure,None if name=='pids' else arguments[0]==pid);raise
            if name=='pids':
                value.update(candidateListed=pid in result,candidateCount=len(result))
            elif arguments[0]==pid:
                if name=='bsd':
                    value['targetBSDReads']+=1
                    if result is None:value['targetGoneReads']+=1
                    else:update_bool('targetBirthMatched',
                        result.pid==pid and f'{result.start_sec}.{result.start_usec:06}'==birth)
                elif name=='path':
                    value['targetPathReads']+=1;update_bool('targetPathMatched',result==executable)
                else:
                    value['targetArgvReads']+=1;update_bool('targetArgvMatched',tuple(result)==args)
            return result
        return call
    lifetime(pid,value,'before')
    try:
        with mock.patch.object(inventory,'pids',wrap('pids')),mock.patch.object(inventory,'bsd',wrap('bsd')), \
             mock.patch.object(inventory,'path',wrap('path')),mock.patch.object(inventory,'argv',wrap('argv')):
            rows=inventory.rows(executable=executable)
        target=[row for row in rows if row[0]==pid]
        value.update(filteredCount=len(rows),targetFiltered=bool(target),exactRowIncluded=expected in rows)
        if target:
            value.update(filteredBirthMatched=all(row[1]==birth for row in target),
                         filteredArgvMatched=all(row[2]==args for row in target))
        return rows
    except Exception as failure:
        if value['selectionAPI'] is None:error('rows',failure)
        raise
    finally:lifetime(pid,value,'after')
