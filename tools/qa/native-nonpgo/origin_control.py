# SPDX-License-Identifier: MPL-2.0
"""Controlled orphan/exec observation. Child exits on pipe EOF or a fixed timeout."""
import os
import subprocess
import sys
import time

from census_diagnostics import identity
from darwin_activity import probe
from darwin_origin import capture,describe_origin
from support import Failure,Runner,clean_environment,require


CHILD = r'''
import os,pathlib,select,sys,time
fd=int(sys.argv[1]); root=pathlib.Path(sys.argv[2]); phase=sys.argv[3]
deadline=time.monotonic()+30
if phase=='fork':
    pid=os.fork()
    if pid:
        (root/'origin-grandchild').write_text(str(pid))
        while not (root/'origin-release-parent').exists() and time.monotonic()<deadline:
            time.sleep(.02)
        os._exit(0)
    os.set_inheritable(fd,True)
(root/('origin-'+phase+'-ready')).write_text('ready')
while time.monotonic()<deadline:
    readable,_,_=select.select([fd],[],[],.2)
    if not readable: continue
    command=os.read(fd,1)
    if command in (b'',b'q'): break
    if command==b'e':
        os.execv(sys.executable,[sys.executable,'-c',sys.argv[4],str(fd),str(root),'exec'])
os._exit(0)
'''


def wait_ready(predicate,code,seconds=5):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if predicate():
            return
        time.sleep(.02)
    raise Failure(code)


def cleanup(parent,write_fd,receipt):
    receipt.update(pipeClosed=False,leaderReaped=False,groupCleanup='unverified',cleanupFailure=None)
    operation='close-control-pipe'
    try:
        os.close(write_fd)
        receipt['pipeClosed']=True
        operation='wait-parent'
        try:
            parent.wait(timeout=5)
        except subprocess.TimeoutExpired:
            # Exact Popen child only; the orphan exits independently on EOF.
            operation='terminate-parent'
            parent.terminate()
            parent.wait(timeout=5)
        operation='wait-group'
        wait_ready(lambda:not Runner.group_alive(parent.pid),'origin-control-group-present')
        receipt['groupCleanup']='verified-gone'
    except BaseException as error:
        receipt['cleanupFailure']={'operation':operation,'kind':
            'permission' if isinstance(error,PermissionError) else
            'timeout' if isinstance(error,(subprocess.TimeoutExpired,Failure)) else 'exception'}
    finally:
        receipt['leaderReaped']=parent.returncode is not None


def run_control(task,result):
    reader=probe()
    own_state,own,_=reader.bsd(os.getpid())
    require(own_state=='measured','origin-control-self')
    own_info,_=capture(identity(own),reader)
    read_fd,write_fd=os.pipe()
    parent=None
    result.update(status='failed',rawProcessMetadataPublished=False,
                  checkpointEligibilityNotEvaluated=True,cleanup={})
    try:
        parent=subprocess.Popen([sys.executable,'-c',CHILD,str(read_fd),str(task),'fork',CHILD],
                                pass_fds=(read_fd,),start_new_session=True,
                                env={**clean_environment(task),'PYTHONDONTWRITEBYTECODE':'1'},
                                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        os.close(read_fd)
        read_fd=None
        wait_ready(lambda:(task/'origin-grandchild').is_file() and (task/'origin-fork-ready').is_file(),
                   'origin-control-fork-ready')
        pid=int((task/'origin-grandchild').read_text())
        parent_state,parent_bsd,_=reader.bsd(parent.pid)
        require(parent_state=='measured','origin-control-parent')
        parent_info,_=capture(identity(parent_bsd),reader)
        require(parent_info.original_parent_version==own_info.idversion and
                parent_info.parent_uniqueid==own_info.uniqueid,'origin-control-direct-child')
        state,bsd,_=reader.bsd(pid)
        require(state=='measured' and bsd.ppid==parent.pid and bsd.pgid==parent.pid and
                bsd.uid==os.geteuid() and bsd.ruid==os.getuid(),'origin-control-grandchild')
        row=identity(bsd)
        before,_=capture(row,reader)
        require(before.original_parent_version==parent_info.idversion and
                before.parent_uniqueid==parent_info.uniqueid,'origin-control-original-parent')
        result['beforeOrphan']={'directChildParentBound':True,'grandchildParentBound':True,
                                'hint':describe_origin(row,set(),reader)}
        (task/'origin-release-parent').write_text('release')
        parent.wait(timeout=5)
        def orphaned():
            state,value,_=reader.bsd(pid)
            return state=='measured' and identity(value)==row and value.ppid==1
        wait_ready(orphaned,'origin-control-reparent')
        orphan,_=capture(row,reader)
        require(orphan.uniqueid==before.uniqueid and orphan.idversion==before.idversion and
                orphan.original_parent_version==before.original_parent_version,'origin-control-orphan-binding')
        result['afterOrphan']={'sameIdentity':True,'originalParentRetained':True,
                               'hint':describe_origin(row,set(),reader)}
        os.write(write_fd,b'e')
        wait_ready(lambda:(task/'origin-exec-ready').is_file(),'origin-control-exec-ready')
        after,_=capture(row,reader)
        require(after.uniqueid==before.uniqueid and after.idversion!=before.idversion and
                after.original_parent_version==before.original_parent_version,'origin-control-exec-binding')
        hint=describe_origin(row,set(),reader)
        require(hint['status']=='identity-verified' and hint['currentParentIsPID1'] and
                hint['parentUniqueMatchesPID1'] is True and
                hint['originalParentVersionMatchesPID1'] is False and
                hint['classification']=='original-parent-not-established' and
                not hint['exclusionAuthorized'],'origin-control-exec-false-positive')
        result['afterExec']={'sameIdentity':True,'execVersionChanged':True,
                            'originalParentRetained':True,'hint':hint}
        result['status']='passed'
    finally:
        already_failing=sys.exc_info()[0] is not None
        if read_fd is not None:
            os.close(read_fd)
        if parent is None:
            os.close(write_fd)
        else:
            cleanup(parent,write_fd,result['cleanup'])
            if (not result['cleanup']['leaderReaped'] or result['cleanup']['groupCleanup']!='verified-gone' or
                    result['cleanup']['cleanupFailure'] is not None):
                result['status']='failed'
                if not already_failing:
                    raise Failure('origin-control-cleanup')
