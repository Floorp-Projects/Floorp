# SPDX-License-Identifier: MPL-2.0
"""Optional original-parent consistency hints. Never establishes a non-writer.

ABI: Apple XNU xnu-11417.101.15 bsd/sys/proc_info_private.h,
proc_uniqidentifierinfo (API size 56), PROC_PIDUNIQIDENTIFIERINFO=17.
Only ordinary live lookup (arg=0). No denied-query fallback or signal authority.
"""
import ctypes
from pathlib import Path
import os

from census_diagnostics import identity
from support import Failure


class UniqueInfo(ctypes.Structure):
    _fields_ = [('uuid',ctypes.c_uint8*16),('uniqueid',ctypes.c_uint64),
                ('parent_uniqueid',ctypes.c_uint64),('idversion',ctypes.c_int32),
                ('original_parent_version',ctypes.c_int32),
                ('reserved2',ctypes.c_uint64),('reserved3',ctypes.c_uint64)]


def key(value):
    return (bytes(value.uuid),value.uniqueid,value.parent_uniqueid,
            value.idversion,value.original_parent_version)


def unique(reader,pid,details):
    if (ctypes.sizeof(UniqueInfo),UniqueInfo.uniqueid.offset,UniqueInfo.idversion.offset,
            UniqueInfo.reserved3.offset)!=(56,16,32,48):
        raise Failure('origin-abi')
    value=UniqueInfo()
    size,code=reader.query(pid,17,value)
    details.update(bytes=size,expectedBytes=56,errno=code)
    if size!=56 or code!=0:
        raise Failure('origin-unique-unavailable')
    if not value.uniqueid or not any(value.uuid) or value.reserved2 or value.reserved3:
        raise Failure('origin-unique-invalid')
    return value


def pid_one_snapshot():
    from process_census import load_abi,snapshot
    return snapshot(load_abi(Path(os.environ['FLOORP_CENSUS_ABI'])),include_pid_one=True)['pidOne']


def capture(row,reader):
    """Return private live identity and exec anchor, bound by two BSD reads."""
    state,before,_=reader.bsd(row[0])
    if state!='measured' or before is None or identity(before)!=row:
        raise Failure('origin-candidate-unavailable')
    first=unique(reader,row[0],{})
    second=unique(reader,row[0],{})
    state,after,_=reader.bsd(row[0])
    if (state!='measured' or after is None or identity(after)!=row or
            (before.ppid,before.pgid)!=(after.ppid,after.pgid) or key(first)!=key(second)):
        raise Failure('origin-candidate-changed')
    return first,before.ppid


def describe_origin(row,observed_births,reader=None,anchor_reader=None):
    result={'status':'unavailable','classification':None,'parentUniqueMatchesPID1':None,
            'originalParentVersionMatchesPID1':None,'observedInStageGroup':
            'observed' if row[:3] in observed_births else 'not-observed',
            'ancestryComplete':False,'writerRoleEstablished':False,
            'exclusionAuthorized':False,'signalOwnershipGranted':False}
    try:
        if reader is None:
            from darwin_activity import probe
            reader=probe()
        anchor_reader=anchor_reader or pid_one_snapshot
        anchor_before=anchor_reader()
        if anchor_before[0]!=1 or anchor_before[3:]!=(0,0):
            raise Failure('origin-anchor-credentials')
        first=unique(reader,1,result.setdefault('anchorQuery',{}))
        candidate,ppid=capture(row,reader)
        second=unique(reader,1,{})
        anchor_after=anchor_reader()
        if anchor_before!=anchor_after or key(first)!=key(second):
            raise Failure('origin-anchor-changed')
        parent=candidate.parent_uniqueid==first.uniqueid
        original=candidate.original_parent_version==first.idversion
        result.update(status='identity-verified',parentUniqueMatchesPID1=parent,
                      originalParentVersionMatchesPID1=original,
                      currentParentIsPID1=ppid==1,
                      classification=('pid1-original-parent-consistent' if ppid==1 and parent and original
                                      else 'original-parent-not-established'))
    except Exception:
        # Unknown remains unknown; do not expose exception text or raw kernel IDs.
        result['status']='diagnostic-unavailable'
    return result
