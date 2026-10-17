# SPDX-License-Identifier: MPL-2.0
"""Observe only the exact private Shim instance; never signal or modify it."""

import ctypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import select
import stat
import subprocess
import sys
import threading
import tempfile
import time

EXITSTATUS = 0x04000000
EXIT_DETAIL = 0x02000000


def decode_exit(data, flags):
    result = {"exitCode": None, "signal": None, "exitStatusAvailable": bool(flags & EXITSTATUS)}
    if flags & EXITSTATUS:
        status = data & 0xffff
        if os.WIFEXITED(status):
            result["exitCode"] = os.WEXITSTATUS(status)
        elif os.WIFSIGNALED(status):
            result["signal"] = os.WTERMSIG(status)
    if flags & EXIT_DETAIL:
        result["codeSigningKill"] = bool(data & 0x00040000)
        result["memoryKill"] = bool(data & 0x00020000)
        result["decryptFailure"] = bool(data & 0x00010000)
    return result


class BSDInfo(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in
                ("flags", "status", "xstatus", "pid", "ppid", "uid", "gid", "ruid", "rgid", "svuid", "svgid", "reserved")]
    _fields_ += [("comm", ctypes.c_char * 16), ("name", ctypes.c_char * 32)]
    _fields_ += [(name, ctypes.c_uint32) for name in ("nfiles", "pgid", "pjobc", "tdev", "tpgid")]
    _fields_ += [("nice", ctypes.c_int32), ("start_sec", ctypes.c_uint64), ("start_usec", ctypes.c_uint64)]


class ShimMonitor:
    def __init__(self, executable, host_pid, process_rows):
        self.executable = str(Path(executable).resolve())
        self.host_pid = host_pid
        self.process_rows = process_rows
        self.lock = threading.Lock()
        self.done = threading.Event()
        self.events = []
        self.pid = None
        self.identity = None
        self.status = "not-started"
        self.thread = None
        self.started_wall = time.time()

    def mark(self, kind, **values):
        with self.lock:
            self.events.append({"kind":kind, "monotonicNs":time.monotonic_ns(),
                                "wallTimeNs":time.time_ns(), **values})
            self.events = self.events[-64:]

    def start(self):
        self.mark("watcher-start")
        if sys.platform != "darwin":
            self.status = "unsupported-platform"
            return
        self.lib = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
        self.lib.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        self.lib.proc_pidpath.restype = ctypes.c_int
        self.lib.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
        self.lib.proc_pidinfo.restype = ctypes.c_int
        self.thread = threading.Thread(target=self._observe, daemon=True)
        self.thread.start()

    def _identity(self, pid):
        path = ctypes.create_string_buffer(4096)
        if self.lib.proc_pidpath(pid, path, len(path)) <= 0 or os.fsdecode(path.value) != self.executable:
            return None
        info = BSDInfo()
        if self.lib.proc_pidinfo(pid, 3, 0, ctypes.byref(info), ctypes.sizeof(info)) != ctypes.sizeof(info):
            return None
        if info.pid != pid or info.uid != os.geteuid():
            return None
        if info.start_sec + info.start_usec / 1000000 < self.started_wall:
            return None
        rows = self.process_rows(pid)
        if len(rows) != 1 or not rows[0][2].startswith(self.executable + " "):
            return None
        arguments = rows[0][2][len(self.executable)+1:].split()
        if len(arguments) > 16:
            return None
        for flag, pattern in (("--host-service", r"\S{1,256}"),
                              ("--host-pid", str(self.host_pid)),
                              ("--launch-token", r"[0-9a-f]{64}")):
            if arguments.count(flag) != 1:
                return None
            index = arguments.index(flag)
            if index+1 >= len(arguments) or not re.fullmatch(pattern, arguments[index+1]):
                return None
        return (pid, info.start_sec, info.start_usec, hashlib.sha256(rows[0][2].encode()).digest())

    def _observe(self):
        queue = None
        try:
            self.status = "discovering"
            deadline = time.monotonic() + 35
            while not self.done.is_set() and self.pid is None and time.monotonic() < deadline:
                pids = subprocess.run(["/bin/ps", "-axo", "pid="], capture_output=True, text=True,
                                      check=True, timeout=2).stdout
                if len(pids) > 65536:
                    raise RuntimeError("process-list-limit")
                matches = [identity for value in pids.split() if value.isdecimal()
                           if (identity := self._identity(int(value))) is not None]
                if len(matches) > 1:
                    raise RuntimeError("ambiguous-instance")
                if matches:
                    self.identity = matches[0]
                    self.pid = self.identity[0]
                    self.mark("owned-pid-discovered", pid=self.pid)
                    queue = select.kqueue()
                    queue.control([select.kevent(self.pid, filter=select.KQ_FILTER_PROC,
                        flags=select.KQ_EV_ADD | select.KQ_EV_ONESHOT,
                        fflags=select.KQ_NOTE_EXIT | EXITSTATUS | EXIT_DETAIL)], 0, 0)
                    if self._identity(self.pid) != self.identity:
                        raise RuntimeError("identity-changed")
                    self.status = "monitoring"
                    self.mark("exit-monitor-registered", pid=self.pid, identityVerified=True,
                              processStartWallTimeUs=self.identity[1]*1000000+self.identity[2])
                else:
                    self.done.wait(0.05)
            while queue is not None and not self.done.is_set():
                for event in queue.control(None, 1, 0.1):
                    if event.ident == self.pid and event.fflags & select.KQ_NOTE_EXIT:
                        self.mark("kernel-exit-observed", pid=self.pid, **decode_exit(event.data, event.fflags))
                        self.status = "exit-observed"
                        return
            if self.pid is None:
                self.status = "not-observed"
        except Exception as error:
            self.status = "unavailable"
            self.mark("observation-error", errorType=type(error).__name__,
                      errno=error.errno if isinstance(error, OSError) else None)
        finally:
            if queue is not None:
                queue.close()

    def snapshot(self):
        with self.lock:
            events = [dict(e) for e in self.events]
        return {"status":self.status, "pid":self.pid, "events":events,
                "kernelTimestampAvailable":False,
                "timeMeaning":"Times are local observation times; browser notifications are asynchronous.",
                "nativeAuthenticationCompleted":None}

    def close(self):
        self.done.set()
        if self.thread is not None:
            self.thread.join(3)
            if self.thread.is_alive():
                self.status = "unavailable"
                self.mark("watcher-stop-timeout")
        self.mark("watcher-stop")


def crash_summary(directory, pid, executable, started_wall, expected_start=None):
    matches = []
    candidates = sorted(Path(directory).glob("floorp-app-shim*.ips"), reverse=True)[:32]
    for path in candidates:
        try:
            descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
            with os.fdopen(descriptor, "rb") as stream:
                info = os.fstat(stream.fileno())
                if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or info.st_size > 2*1024*1024 or info.st_mtime < started_wall:
                    continue
                data = stream.read(2*1024*1024+1)
            if len(data) > 2*1024*1024:
                continue
            text = data.decode("utf-8")
            decoder = json.JSONDecoder()
            _header, end = decoder.raw_decode(text)
            report = json.loads(text[end:].strip()) if text[end:].strip() else _header
            if not isinstance(report, dict):
                continue
            if type(report.get("pid")) is not int or report["pid"] <= 0 or report.get("procPath") != str(executable):
                continue
            if pid is not None and report["pid"] != pid:
                continue
            start = datetime.datetime.strptime(report.get("procLaunch", ""), "%Y-%m-%d %H:%M:%S.%f %z").timestamp()
            if start < started_wall or start > time.time() or (expected_start is not None and abs(start-expected_start) > 0.01):
                continue
            termination = report.get("termination", {})
            exception = report.get("exception", {})
            if not isinstance(termination, dict):
                termination = {}
            if not isinstance(exception, dict):
                exception = {}
            item = {"matchedOwnedPID":pid is not None, "matchedPrivateExecutable":True,
                    "startTimeMatched":expected_start is not None, "pid":report["pid"]}
            for source,key,target,allowed in [
                (termination,"namespace","terminationNamespace",{"SIGNAL","DYLD","CODESIGNING","LIBSYSTEM","RUNNINGBOARD"}),
                (exception,"type","exceptionType",{"EXC_BAD_ACCESS","EXC_CRASH","EXC_GUARD","EXC_BREAKPOINT","EXC_RESOURCE"}),
                (exception,"signal","exceptionSignal",{"SIGABRT","SIGSEGV","SIGILL","SIGTRAP","SIGKILL","SIGBUS"})]:
                value=source.get(key)
                if isinstance(value,str):item[target]=value if value in allowed else "other"
            if type(termination.get("code")) is int:item["terminationCode"]=termination["code"]
            diagnostic = json.dumps(report.get("asi", {})) + json.dumps(termination)
            item["markers"] = [name for literal,name in [
                ("Library not loaded:","loader-library-not-loaded"),("libmozglue.dylib","mozglue-mentioned"),
                ("Symbol not found:","loader-symbol-not-found"),("Code Signature Invalid","invalid-code-signature")]
                if literal in diagnostic]
            matches.append(item)
        except (ValueError, OSError, TypeError):
            continue
    return {"matchedReports":matches[:4], "reportAbsenceDoesNotExcludeCrash":True,
            "rawContentPublished":False}


def binary_summary(executable):
    result = {}
    for arch in ("x86_64", "arm64"):
        process = subprocess.run(["/usr/bin/otool", "-arch", arch, "-L", str(executable)],
                                 capture_output=True, text=True, timeout=5, check=False)
        libraries = []
        for line in process.stdout.splitlines()[1:]:
            value = line.strip().split(" (", 1)[0]
            name = value.rsplit("/", 1)[-1]
            if re.fullmatch(r"[A-Za-z0-9_.+\\-]{1,80}", name):
                known = {"Cocoa", "AppKit", "Foundation", "Security", "QuartzCore", "IOSurface", "Carbon",
                         "CoreFoundation", "libSystem.B.dylib", "libc++.1.dylib", "libobjc.A.dylib",
                         "libbsm.0.dylib", "libmozglue.dylib", "XUL", "libnspr4.dylib", "libnss3.dylib"}
                libraries.append({"name":name if name in known else "other", "pathKind":"system" if value.startswith(("/System/", "/usr/lib/"))
                                  else "rpath" if value.startswith("@rpath/") else "other"})
        result[arch] = {"inspectionExitCode":process.returncode, "libraries":libraries[:32]}
    return {"libraries":result, "rawContentPublished":False}


SYSTEM_MARKERS = {
    "code signature invalid":"invalid-code-signature",
    "failed to validate code signature":"code-signature-validation-failed",
    "seccodecheckvalidity":"code-validity-call",
    "bootstrap_look_up":"bootstrap-lookup-call",
    "mach-lookup":"mach-lookup-mentioned",
    "windowserver":"windowserver-mentioned",
    "library not loaded:":"loader-library-not-loaded",
    "failed to launch":"launch-failed",
    "connection invalid":"connection-invalid",
}


def system_log_records(text, identities, start, end):
    result=[]
    matching_count=0
    line_count=0
    for line in text.splitlines():
        line_count+=1
        try:
            row=json.loads(line)
            if not isinstance(row,dict) or type(row.get("processID")) is not int or row["processID"] not in identities:
                continue
            if row.get("processImagePath") != identities[row["processID"]]:
                continue
            timestamp=row.get("timestamp")
            if not isinstance(timestamp,str):
                continue
            stamp=datetime.datetime.fromisoformat(timestamp.replace(" +", "+").replace(" -", "-"))
            if stamp.tzinfo is None or not start <= stamp.timestamp() <= end:
                continue
            message=row.get("eventMessage")
            if not isinstance(message,str):
                continue
            markers=[name for literal,name in SYSTEM_MARKERS.items() if literal in message.lower()]
            if markers:
                matching_count+=1
                result.append({"pid":row["processID"],"timeMs":int(stamp.timestamp()*1000),"markers":markers})
                result=result[-64:]
        except (ValueError,TypeError):
            continue
    return {"events":result,"matchingEventCount":matching_count,"inputLineCount":line_count,
            "eventRingTruncated":matching_count>64}


def system_log_summary(private_dir, identities, started_wall, ended_wall):
    identities={pid:path for pid,path in identities.items() if type(pid) is int and pid > 0}
    result={"rawContentPublished":False,"events":[],"permissionEscalated":False}
    if sys.platform != "darwin" or not identities:
        return {**result,"status":"unavailable"}
    predicate=" OR ".join("processID == "+str(pid) for pid in sorted(identities))
    try:
        with tempfile.TemporaryFile(dir=private_dir) as output, tempfile.TemporaryFile(dir=private_dir) as errors:
            process=subprocess.run(["/usr/bin/log","show","--last","2m","--style","ndjson", "--info", "--predicate",predicate],
                                   stdout=output,stderr=errors,timeout=10,check=False)
            output.seek(0);data=output.read(1048577)
            errors.seek(0);error_data=errors.read(4096).decode("utf-8",errors="replace").lower()
        result.update({"commandExitCode":process.returncode,"truncated":len(data)>1048576,
                       "accessDenied":any(value in error_data for value in ("operation not permitted","permission denied","must be root","not authorized"))})
        result["status"]="collected" if process.returncode==0 else "unavailable"
        if process.returncode==0:
            result.update(system_log_records(data[:1048576].decode("utf-8",errors="replace"),identities,started_wall,ended_wall))
            result["truncated"] = result["truncated"] or result["eventRingTruncated"]
    except Exception as error:
        result.update({"status":"unavailable","errorType":type(error).__name__})
    return result
