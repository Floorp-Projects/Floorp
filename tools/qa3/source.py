# SPDX-License-Identifier: MPL-2.0

import argparse
import ctypes
import errno
import hashlib
import json
import math
import os
import platform
import re
import signal
import subprocess
import sys
import time
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GROUPS = {
    "deno-host": (
        "tools/src/smoke_runner.test.ts",
        "tools/src/utils.test.ts",
        "tools/src/colocated_test_runner.test.ts",
    ),
    "node-intake": ("tools/test-final-package-qa-source.mjs",),
    "python-provenance": (
        "tools/test_validate_runtime_source_provenance.py",
        "tools/test_validate_runtime_packaging_provenance.py",
    ),
}
CASE_COUNTS = {"deno-host": 60, "node-intake": 28, "python-provenance": 21}
DENO_IMPORTS = ("@std/assert", "@std/cli", "@std/fs", "@std/path")
TARGETS = [
    "windows-x86_64",
    "linux-x86_64",
    "linux-aarch64",
    "mac-x86_64",
    "mac-aarch64",
]
ARTIFACT_TARGETS = {
    "installer": TARGETS,
    "mar": TARGETS,
    "deb-ppa": ["linux-x86_64"],
    "portable": ["windows-x86_64"],
    "stub": ["windows-x86_64"],
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON field")
            result[key] = value
        return result

    data = path.read_bytes()
    if len(data) > 1024 * 1024:
        raise ValueError("oversized policy")
    return json.loads(data, object_pairs_hook=unique)


def validate_plan(plan):
    if not isinstance(plan, dict):
        raise TypeError("plan must be an object")
    if set(plan) != {
        "schemaVersion",
        "id",
        "stage",
        "origin",
        "executionKind",
        "target",
        "groups",
    }:
        raise ValueError("unknown or missing plan field")
    if type(plan["schemaVersion"]) is not int or plan["schemaVersion"] != 1:
        raise ValueError("unsupported plan schema")
    for key, value in {
        "id": "qa3-source-host-v1",
        "stage": "shadow",
        "origin": "floorp",
        "executionKind": "host-unit",
        "target": "linux-x86_64-host",
    }.items():
        if plan[key] != value:
            raise ValueError(f"invalid plan {key}")
    if (
        not isinstance(plan["groups"], list)
        or not all(isinstance(g, dict) for g in plan["groups"])
        or [g.get("id") for g in plan["groups"]] != list(GROUPS)
    ):
        raise ValueError("fixed source subset changed")
    for group in plan["groups"]:
        if set(group) != {"id", "paths", "cases"} or group["paths"] != list(
            GROUPS[group["id"]]
        ):
            raise ValueError("unregistered source command or file")
        cases = group["cases"]
        if not isinstance(cases, list) or len(cases) != CASE_COUNTS[group["id"]]:
            raise ValueError("empty or reduced source test plan")
        ids = []
        for case in cases:
            if (
                not isinstance(case, dict)
                or set(case) != {"file", "name"}
                or case["file"] not in group["paths"]
            ):
                raise ValueError("unregistered test origin")
            if not isinstance(case["name"], str) or not 0 < len(case["name"]) <= 1024:
                raise ValueError("invalid test name")
            ids.append((case["file"], case["name"]))
        if len(set(ids)) != len(ids) or {f for f, _ in ids} != set(group["paths"]):
            raise ValueError("duplicate test or missing source file")
    return plan


def validate_coverage(catalog):
    if not isinstance(catalog, dict):
        raise TypeError("coverage must be an object")
    if set(catalog) != {
        "schemaVersion",
        "stage",
        "publicationAuthorized",
        "artifacts",
        "deferred",
    }:
        raise ValueError("invalid coverage catalog")
    if type(catalog["schemaVersion"]) is not int or catalog["schemaVersion"] != 1:
        raise ValueError("invalid coverage schema")
    if catalog["stage"] != "shadow" or catalog["publicationAuthorized"] is not False:
        raise ValueError("source evidence cannot authorize publication")
    entries = catalog["artifacts"]
    if (
        not isinstance(entries, list)
        or not all(isinstance(x, dict) for x in entries)
        or [x.get("kind") for x in entries] != list(ARTIFACT_TARGETS)
    ):
        raise ValueError("missing publication artifact kind")
    for entry in entries:
        if set(entry) != {"kind", "targets", "verdict", "requires", "producer"}:
            raise ValueError("invalid artifact coverage")
        if (
            entry["targets"] != ARTIFACT_TARGETS[entry["kind"]]
            or entry["verdict"] != "UNVERIFIED"
        ):
            raise ValueError("missing target or unsupported product PASS")
        if (
            not isinstance(entry["requires"], list)
            or not entry["requires"]
            or not all(isinstance(x, str) and x for x in entry["requires"])
            or not isinstance(entry["producer"], str)
            or not entry["producer"]
        ):
            raise ValueError("artifact coverage requires an explicit proof plan")
    if (
        not isinstance(catalog["deferred"], list)
        or not catalog["deferred"]
        or not all(isinstance(x, str) and x for x in catalog["deferred"])
    ):
        raise ValueError("deferred capabilities must remain visible")
    return catalog


def validate_cases(group, actual):
    if not isinstance(actual, list) or not all(
        isinstance(c, dict)
        and set(c) == {"file", "name", "verdict"}
        and all(isinstance(v, str) for v in c.values())
        for c in actual
    ):
        raise ValueError("invalid executed case evidence")
    expected = {(c["file"], c["name"]) for c in group["cases"]}
    observed = [(c["file"], c["name"]) for c in actual]
    if len(observed) != len(set(observed)) or set(observed) != expected:
        raise ValueError("missing, duplicate, or unexpected executed test")
    if any(c["verdict"] != "PASS" for c in actual):
        raise ValueError("failed, skipped, or incomplete source test")


def validate_deno_inputs(host_map, host_lock, repo_config, repo_lock):
    try:
        imports = {key: repo_config["imports"][key] for key in DENO_IMPORTS}
        expected_lock = {
            "version": repo_lock["version"],
            "specifiers": {
                key: value
                for key, value in repo_lock["specifiers"].items()
                if key.startswith("jsr:")
            },
            "jsr": repo_lock["jsr"],
        }
    except (KeyError, TypeError, AttributeError) as error:
        raise ValueError("missing repository Deno host inputs") from error
    if not all(
        isinstance(value, str) and value.startswith("jsr:@std/")
        for value in imports.values()
    ):
        raise ValueError(
            "host imports must remain the repository's standard-library aliases"
        )
    if host_map != {"imports": imports} or host_lock != expected_lock:
        raise ValueError("host import map or JSR lock differs from repository inputs")


def audit_deno_subset(repo):
    for path in GROUPS["deno-host"]:
        text = (repo / path).read_text()
        definitions = re.findall(
            r'Deno\.test\(\s*"(?:\\.|[^"\\])*"\s*,\s*(?:async\s+)?(?:\(|function\b)',
            text,
        )
        if (
            len(definitions) != len(re.findall(r"\bDeno\.test\s*\(", text))
            or not definitions
        ):
            raise ValueError(
                "host subset requires plain tests without per-test retry/ignore options"
            )


def read_junit(path, repo, node_file=None):
    if node_file is not None and node_file not in GROUPS["node-intake"]:
        raise ValueError("unregistered single-file Node report origin")
    data = path.read_bytes()
    if not data or len(data) > 4 * 1024 * 1024 or b"\0" in data:
        raise ValueError("missing, oversized, or unsafe JUnit report")
    text = data.decode("utf-8-sig")
    encoding = re.search(
        r"<\?xml\s+[^?]*\bencoding\s*=\s*(['\"])(.*?)\1", text, re.IGNORECASE
    )
    if (
        re.search(r"<!\s*(?:DOCTYPE|ENTITY)", text, re.IGNORECASE)
        or encoding
        and encoding[2].lower() not in {"utf-8", "utf8"}
    ):
        raise ValueError("unsafe or non-UTF-8 JUnit report")
    root = ET.fromstring(text)
    if root.tag not in {"testsuites", "testsuite"}:
        raise ValueError("unsupported JUnit document")
    for suite in root.iter():
        if suite.tag not in {"testsuites", "testsuite"}:
            continue
        for key in ("failures", "errors", "skipped", "disabled"):
            if key in suite.attrib and suite.attrib[key] != "0":
                raise ValueError("failed or skipped JUnit aggregate")
        if "tests" in suite.attrib and suite.attrib["tests"] != str(
            len(list(suite.iter("testcase")))
        ):
            raise ValueError("JUnit aggregate execution mismatch")
    cases = []
    for case in root.iter("testcase"):
        filename = case.get("file") or case.get("classname", "")
        if "file" not in case.attrib and filename == "test" and node_file is not None:
            filename = node_file
        file = Path(filename)
        if file.is_absolute():
            filename = file.relative_to(repo).as_posix()
        else:
            filename = file.as_posix().removeprefix("./")
        if case.get("status", "passed") not in {"passed", "run"} or any(
            child.tag not in {"system-out", "system-err"} for child in case
        ):
            verdict = "FAIL"
        else:
            verdict = "PASS"
        cases.append(
            {
                "file": filename,
                "name": case.get("name", ""),
                "verdict": verdict,
            }
        )
    return cases


def python_tests(output):
    sys.path.insert(0, str(ROOT / "tools"))
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for path in GROUPS["python-provenance"]:
        suite.addTests(loader.loadTestsFromName(Path(path).stem))
    cases = []

    class Result(unittest.TextTestResult):
        def startTest(self, test):
            self.current = {
                "file": f"tools/{test.id().split('.')[0]}.py",
                "name": test.id(),
                "verdict": "INCOMPLETE",
            }
            cases.append(self.current)
            super().startTest(test)

        def addSuccess(self, test):
            self.current["verdict"] = "PASS"
            super().addSuccess(test)

        def addSubTest(self, test, subtest, err):
            if err is not None:
                self.current["verdict"] = "FAIL"
            super().addSubTest(test, subtest, err)

    result = unittest.TextTestRunner(verbosity=2, resultclass=Result).run(suite)
    output.write_text(json.dumps(cases, indent=2) + "\n")
    return (
        0
        if result.wasSuccessful()
        and not result.skipped
        and not result.unexpectedSuccesses
        else 1
    )


def child_environment():
    return {
        key: os.environ[key]
        for key in (
            "PATH",
            "HOME",
            "TMPDIR",
            "TEMP",
            "SYSTEMROOT",
            "DENO_DIR",
        )
        if key in os.environ
    } | {
        "PYTHONDONTWRITEBYTECODE": "1",
        "LC_ALL": "C.UTF-8",
        "DENO_NO_PACKAGE_JSON": "1",
    }


def _process_info(pid):
    try:
        fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return int(fields[1]), fields[0], int(fields[19])
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None


def _process_snapshot():
    result = {}
    for entry in Path("/proc").iterdir():
        if not entry.name.isdecimal():
            continue
        info = _process_info(int(entry.name))
        if info:
            result[int(entry.name)] = info
    return result


def _owned_descendants(owned):
    snapshot = _process_snapshot()
    ancestors = {
        pid
        for pid, birth in owned.items()
        if pid in snapshot and snapshot[pid][2] == birth
    } | {os.getpid()}
    while True:
        children = {
            pid for pid, info in snapshot.items() if info[0] in ancestors
        } - ancestors
        if not children:
            break
        for pid in children:
            owned[pid] = snapshot[pid][2]
        ancestors.update(children)
    return {
        pid: snapshot[pid]
        for pid, birth in owned.items()
        if pid in snapshot and snapshot[pid][2] == birth
    }


def _cleanup_owned(process, owned):
    errors = []

    def record(error):
        message = _bounded_error(error)
        if message not in errors and len(errors) < 4:
            errors.append(message)

    for sig, interval in ((signal.SIGTERM, 2), (signal.SIGKILL, 2)):
        deadline = time.monotonic() + interval
        while True:
            try:
                alive = _owned_descendants(owned)
            except OSError as error:
                record(error)
                alive = {}
            try:
                if process.poll() is None:
                    process.send_signal(sig)
            except OSError as error:
                record(error)
            for pid, info in alive.items():
                if info[1] != "Z":
                    try:
                        fd = os.pidfd_open(pid)
                    except ProcessLookupError:
                        continue
                    except OSError as error:
                        record(error)
                        continue
                    try:
                        current = _process_info(pid)
                        if current and current[2] == info[2]:
                            signal.pidfd_send_signal(fd, sig)
                    except ProcessLookupError:
                        pass
                    except OSError as error:
                        record(error)
                    finally:
                        try:
                            os.close(fd)
                        except OSError as error:
                            record(error)
            try:
                process.poll()
            except OSError as error:
                record(error)
            for pid in alive:
                if pid == process.pid:
                    continue
                try:
                    os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    pass
                except OSError as error:
                    record(error)
            try:
                remaining = _owned_descendants(owned)
            except OSError as error:
                record(error)
                remaining = True
            if not remaining and process.returncode is not None:
                break
            if time.monotonic() >= deadline:
                break
            time.sleep(0.02)
        if not remaining and process.returncode is not None:
            break
    try:
        process.wait(timeout=1)
    except (OSError, subprocess.TimeoutExpired) as error:
        record(error)
    try:
        complete = not _owned_descendants(owned) and process.returncode is not None
    except OSError as error:
        record(error)
        complete = False
    return complete, errors


def _bounded_error(error):
    text = f"{type(error).__name__}: {error}"
    text = "".join(c if c.isprintable() else " " for c in text)
    return text.encode("utf-8", errors="replace")[:256].decode("utf-8", errors="ignore")


# --- owned-process supervisors -------------------------------------------
#
# QA3 makes three promises about the command it supervises:
#
# 1. every descendant is collected before the supervisor exits;
# 2. unrelated processes are never signalled;
# 3. incomplete cleanup is recorded and a recorded cleanup error fails the run.
#
# Linux implements them with a subreaper, birth identities and pidfds. macOS
# has none of those; it implements the contract with a session (below). Windows
# implements the same contract with a Job Object: the supervisor joins the job
# before it spawns the command, so membership is inherited without a race,
# termination is gated on a job-membership check plus a process creation
# identity, and JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE is the final safety net. The
# job handle stays open until the supervisor process exits, so the verdict is
# decided by the explicit cleanup loop and its verification, above the net.

WINDOWS_JOB_KILL_ON_JOB_CLOSE = 0x00002000
WINDOWS_JOB_OBJECT_EXTENDED_LIMIT_INFORMATION = 9

_PROCESS_TERMINATE = 0x0001
_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
_PROCESS_SYNCHRONIZE = 0x00100000
_TH32CS_SNAPPROCESS = 0x00000002
_WAIT_TIMEOUT = 0x00000102
_ERROR_INVALID_PARAMETER = 87
_ERROR_NOT_FOUND = 1168
_INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


class _IoCounters(ctypes.Structure):
    _fields_ = [
        ("ReadOperationCount", ctypes.c_uint64),
        ("WriteOperationCount", ctypes.c_uint64),
        ("OtherOperationCount", ctypes.c_uint64),
        ("ReadTransferCount", ctypes.c_uint64),
        ("WriteTransferCount", ctypes.c_uint64),
        ("OtherTransferCount", ctypes.c_uint64),
    ]


class _JobBasicLimitInformation(ctypes.Structure):
    _fields_ = [
        ("PerProcessUserTimeLimit", ctypes.c_int64),
        ("PerJobUserTimeLimit", ctypes.c_int64),
        ("LimitFlags", ctypes.c_uint32),
        ("MinimumWorkingSetSize", ctypes.c_size_t),
        ("MaximumWorkingSetSize", ctypes.c_size_t),
        ("ActiveProcessLimit", ctypes.c_uint32),
        ("Affinity", ctypes.c_size_t),
        ("PriorityClass", ctypes.c_uint32),
        ("SchedulingClass", ctypes.c_uint32),
    ]


class _JobExtendedLimitInformation(ctypes.Structure):
    _fields_ = [
        ("BasicLimitInformation", _JobBasicLimitInformation),
        ("IoInfo", _IoCounters),
        ("ProcessMemoryLimit", ctypes.c_size_t),
        ("JobMemoryLimit", ctypes.c_size_t),
        ("PeakProcessMemoryUsed", ctypes.c_size_t),
        ("PeakJobMemoryUsed", ctypes.c_size_t),
    ]


class _FileTime(ctypes.Structure):
    _fields_ = [
        ("dwLowDateTime", ctypes.c_uint32),
        ("dwHighDateTime", ctypes.c_uint32),
    ]


class _ProcessEntry32(ctypes.Structure):
    _fields_ = [
        ("dwSize", ctypes.c_uint32),
        ("cntUsage", ctypes.c_uint32),
        ("th32ProcessID", ctypes.c_uint32),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", ctypes.c_uint32),
        ("cntThreads", ctypes.c_uint32),
        ("th32ParentProcessID", ctypes.c_uint32),
        ("pcPriClassBase", ctypes.c_int32),
        ("dwFlags", ctypes.c_uint32),
        ("szExeFile", ctypes.c_uint16 * 260),
    ]


_kernel32 = None


def _win32_kernel():
    global _kernel32
    if _kernel32 is not None:
        return _kernel32
    if os.name != "nt":
        raise OSError("Windows Job Object supervision requires Windows")
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    for name, argument_types, result_type in (
        (
            "CreateJobObjectW",
            [ctypes.c_void_p, ctypes.c_wchar_p],
            ctypes.c_void_p,
        ),
        (
            "SetInformationJobObject",
            [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32],
            ctypes.c_int,
        ),
        (
            "QueryInformationJobObject",
            [
                ctypes.c_void_p,
                ctypes.c_int,
                ctypes.c_void_p,
                ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint32),
            ],
            ctypes.c_int,
        ),
        (
            "AssignProcessToJobObject",
            [ctypes.c_void_p, ctypes.c_void_p],
            ctypes.c_int,
        ),
        ("GetCurrentProcess", [], ctypes.c_void_p),
        (
            "IsProcessInJob",
            [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)],
            ctypes.c_int,
        ),
        (
            "OpenProcess",
            [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32],
            ctypes.c_void_p,
        ),
        ("TerminateProcess", [ctypes.c_void_p, ctypes.c_uint32], ctypes.c_int),
        (
            "WaitForSingleObject",
            [ctypes.c_void_p, ctypes.c_uint32],
            ctypes.c_uint32,
        ),
        ("CloseHandle", [ctypes.c_void_p], ctypes.c_int),
        (
            "CreateToolhelp32Snapshot",
            [ctypes.c_uint32, ctypes.c_uint32],
            ctypes.c_void_p,
        ),
        (
            "Process32FirstW",
            [ctypes.c_void_p, ctypes.POINTER(_ProcessEntry32)],
            ctypes.c_int,
        ),
        (
            "Process32NextW",
            [ctypes.c_void_p, ctypes.POINTER(_ProcessEntry32)],
            ctypes.c_int,
        ),
        (
            "GetProcessTimes",
            [ctypes.c_void_p]
            + [ctypes.POINTER(_FileTime)] * 4,
            ctypes.c_int,
        ),
    ):
        function = getattr(kernel32, name)
        function.argtypes = argument_types
        function.restype = result_type
    _kernel32 = kernel32
    return kernel32


def _win_close(handle):
    if not handle:
        return
    if not _win32_kernel().CloseHandle(handle):
        raise ctypes.WinError(ctypes.get_last_error())


def _win_create_job():
    job = _win32_kernel().CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    return job


def _win_limit_flags(job):
    info = _JobExtendedLimitInformation()
    if not _win32_kernel().QueryInformationJobObject(
        job,
        WINDOWS_JOB_OBJECT_EXTENDED_LIMIT_INFORMATION,
        ctypes.byref(info),
        ctypes.sizeof(info),
        None,
    ):
        raise ctypes.WinError(ctypes.get_last_error())
    return int(info.BasicLimitInformation.LimitFlags)


def _win_set_kill_on_job_close(job):
    info = _JobExtendedLimitInformation()
    info.BasicLimitInformation.LimitFlags = WINDOWS_JOB_KILL_ON_JOB_CLOSE
    if not _win32_kernel().SetInformationJobObject(
        job,
        WINDOWS_JOB_OBJECT_EXTENDED_LIMIT_INFORMATION,
        ctypes.byref(info),
        ctypes.sizeof(info),
    ):
        raise ctypes.WinError(ctypes.get_last_error())
    if not _win_limit_flags(job) & WINDOWS_JOB_KILL_ON_JOB_CLOSE:
        raise OSError("the job object did not retain kill-on-close")


def _win_assign_current_process(job):
    kernel32 = _win32_kernel()
    if not kernel32.AssignProcessToJobObject(job, kernel32.GetCurrentProcess()):
        raise ctypes.WinError(ctypes.get_last_error())


def _win_process_table():
    kernel32 = _win32_kernel()
    snapshot = kernel32.CreateToolhelp32Snapshot(_TH32CS_SNAPPROCESS, 0)
    if not snapshot or snapshot == _INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    table = {}
    try:
        entry = _ProcessEntry32()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            raise ctypes.WinError(ctypes.get_last_error())
        while True:
            table[int(entry.th32ProcessID)] = int(entry.th32ParentProcessID)
            if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                break
    finally:
        _win_close(snapshot)
    return table


def _win_handle_identity(handle):
    created = _FileTime()
    exited = _FileTime()
    kernel = _FileTime()
    user = _FileTime()
    if not _win32_kernel().GetProcessTimes(
        handle,
        ctypes.byref(created),
        ctypes.byref(exited),
        ctypes.byref(kernel),
        ctypes.byref(user),
    ):
        error = ctypes.get_last_error()
        if error in {_ERROR_INVALID_PARAMETER, _ERROR_NOT_FOUND}:
            return None
        raise ctypes.WinError(error)
    return (int(created.dwHighDateTime) << 32) | int(created.dwLowDateTime)


def _win_process_identity(pid):
    handle = _win32_kernel().OpenProcess(
        _PROCESS_QUERY_LIMITED_INFORMATION, False, int(pid)
    )
    if not handle:
        error = ctypes.get_last_error()
        if error in {_ERROR_INVALID_PARAMETER, _ERROR_NOT_FOUND}:
            return None
        raise ctypes.WinError(error)
    try:
        return _win_handle_identity(handle)
    finally:
        _win_close(handle)


def _win_is_alive(pid, identity=None):
    handle = _win32_kernel().OpenProcess(
        _PROCESS_QUERY_LIMITED_INFORMATION | _PROCESS_SYNCHRONIZE,
        False,
        int(pid),
    )
    if not handle:
        error = ctypes.get_last_error()
        if error in {_ERROR_INVALID_PARAMETER, _ERROR_NOT_FOUND}:
            return False
        raise ctypes.WinError(error)
    try:
        if _win32_kernel().WaitForSingleObject(handle, 0) != _WAIT_TIMEOUT:
            return False
        if identity is not None and _win_handle_identity(handle) != identity:
            return False
        return True
    finally:
        _win_close(handle)


def _win_job_members(job):
    self_pid = os.getpid()
    members = set()
    for pid in _win_process_table():
        if pid == self_pid:
            continue
        handle = _win32_kernel().OpenProcess(
            _PROCESS_QUERY_LIMITED_INFORMATION, False, pid
        )
        if not handle:
            continue
        try:
            member = ctypes.c_int()
            if not _win32_kernel().IsProcessInJob(
                handle, job, ctypes.byref(member)
            ):
                continue
            if member.value:
                members.add(pid)
        finally:
            _win_close(handle)
    return members


def _win_terminate(job, pid, identity=None, allow_non_member=False):
    handle = _win32_kernel().OpenProcess(
        _PROCESS_TERMINATE | _PROCESS_QUERY_LIMITED_INFORMATION | _PROCESS_SYNCHRONIZE,
        False,
        int(pid),
    )
    if not handle:
        error = ctypes.get_last_error()
        if error in {_ERROR_INVALID_PARAMETER, _ERROR_NOT_FOUND}:
            return True
        raise ctypes.WinError(error)
    try:
        kernel32 = _win32_kernel()
        if kernel32.WaitForSingleObject(handle, 0) != _WAIT_TIMEOUT:
            return True
        if identity is not None and _win_handle_identity(handle) != identity:
            return True
        member = ctypes.c_int()
        if not kernel32.IsProcessInJob(handle, job, ctypes.byref(member)):
            raise ctypes.WinError(ctypes.get_last_error())
        if not member.value and not allow_non_member:
            return False
        if not kernel32.TerminateProcess(handle, 1):
            error = ctypes.get_last_error()
            if error in {_ERROR_INVALID_PARAMETER, _ERROR_NOT_FOUND}:
                return True
            if kernel32.WaitForSingleObject(handle, 0) != _WAIT_TIMEOUT:
                return True
            raise ctypes.WinError(error)
        kernel32.WaitForSingleObject(handle, 1000)
        return True
    finally:
        _win_close(handle)


class WindowsJobObjectApi:
    """Windows primitives for the owned-process supervisor."""

    def __init__(self):
        if os.name != "nt":
            raise OSError("Windows Job Object supervision requires Windows")
        self.job = None
        _win32_kernel()

    def create_job(self):
        self.close_job()
        self.job = _win_create_job()
        return self.job

    def enable_kill_on_job_close(self):
        _win_set_kill_on_job_close(self.job)

    def limit_flags(self):
        return _win_limit_flags(self.job)

    def assign_current_process(self):
        _win_assign_current_process(self.job)

    def spawn(self, argv):
        return subprocess.Popen(argv, env=child_environment())

    def process_table(self):
        return _win_process_table()

    def identity(self, pid):
        return _win_process_identity(pid)

    def is_alive(self, pid, identity=None):
        return _win_is_alive(pid, identity)

    def job_members(self):
        return _win_job_members(self.job)

    def terminate(self, pid, identity=None, allow_non_member=False):
        return _win_terminate(self.job, pid, identity, allow_non_member)

    def close_job(self):
        job, self.job = self.job, None
        if job:
            _win_close(job)


class WindowsJobSupervisor:
    """Own every descendant on Windows through a kill-on-close Job Object."""

    def __init__(self, api=None, cleanup_interval=2.0):
        self.api = api if api is not None else WindowsJobObjectApi()
        self.cleanup_interval = cleanup_interval
        self.process = None
        self.owned = {}
        self.prepared = False

    def prepare(self):
        self.api.create_job()
        try:
            self.api.enable_kill_on_job_close()
            self.api.assign_current_process()
        except BaseException:
            try:
                self.api.close_job()
            except OSError:
                pass
            raise
        self.prepared = True

    def spawn(self, argv):
        self.process = self.api.spawn(argv)
        identity = self.api.identity(self.process.pid)
        self.owned[self.process.pid] = identity
        return self.process

    def track(self):
        self._collect(membership=False)
        return self.owned

    def _collect(self, membership=True, table=None):
        table = self.api.process_table() if table is None else table
        self_pid = os.getpid()
        changed = True
        while changed:
            changed = False
            for pid, parent in table.items():
                if pid == self_pid or pid in self.owned:
                    continue
                if parent in self.owned:
                    identity = self.api.identity(pid)
                    if identity is not None:
                        self.owned[pid] = identity
                        changed = True
        if membership:
            for pid in self.api.job_members():
                if pid == self_pid or pid in self.owned:
                    continue
                identity = self.api.identity(pid)
                if identity is not None:
                    self.owned[pid] = identity
        self.owned.pop(self_pid, None)
        return self.owned

    def _alive_owned(self):
        self._collect()
        return {
            pid: identity
            for pid, identity in self.owned.items()
            if self.api.is_alive(pid, identity)
        }

    def _stop(self, pid, record):
        if not self.api.terminate(
            pid,
            self.owned.get(pid),
            allow_non_member=pid in self.owned,
        ):
            record(OSError(f"the job object refused to stop owned process {pid}"))

    def cleanup(self):
        errors = []

        def record(error):
            message = _bounded_error(error)
            if message not in errors and len(errors) < 4:
                errors.append(message)

        if self.process is None:
            return True, errors
        if not self.prepared:
            record(OSError("the Windows job object was not prepared"))
            return False, errors
        remaining = dict(self.owned)
        for _ in (1, 2):
            deadline = time.monotonic() + self.cleanup_interval
            while True:
                try:
                    alive = self._alive_owned()
                except OSError as error:
                    record(error)
                    alive = {}
                try:
                    if self.process.poll() is None:
                        self._stop(self.process.pid, record)
                except OSError as error:
                    record(error)
                for pid in sorted(alive):
                    if pid == self.process.pid:
                        continue
                    try:
                        self._stop(pid, record)
                    except OSError as error:
                        record(error)
                try:
                    self.process.poll()
                except OSError as error:
                    record(error)
                try:
                    remaining = self._alive_owned()
                except OSError as error:
                    record(error)
                    remaining = dict(self.owned)
                if not remaining and self.process.returncode is not None:
                    break
                if time.monotonic() >= deadline:
                    break
                time.sleep(0.02)
            if not remaining and self.process.returncode is not None:
                break
        try:
            self.process.wait(timeout=1)
        except (OSError, subprocess.TimeoutExpired) as error:
            record(error)
        try:
            complete = not self._alive_owned() and self.process.returncode is not None
        except OSError as error:
            record(error)
            complete = False
        return complete, errors


class PosixSubreaperSupervisor:
    """Own every descendant on Linux with a subreaper, identities and pidfds."""

    def __init__(self):
        self.process = None
        self.owned = {}
        self.prepared = False

    def prepare(self):
        libc = ctypes.CDLL(None, use_errno=True)
        if libc.prctl(36, 1, 0, 0, 0) != 0:
            raise OSError("cannot own orphaned source descendants")
        self.prepared = True

    def spawn(self, argv):
        self.process = subprocess.Popen(
            ["/usr/bin/prlimit", "--fsize=16777216:16777216", "--", *argv],
            env=child_environment(),
            start_new_session=True,
        )
        return self.process

    def track(self):
        return _owned_descendants(self.owned)

    def cleanup(self):
        return _cleanup_owned(self.process, self.owned)


# --- macOS -------------------------------------------------------------------
#
# macOS has no subreaper, no pidfd and no job object, and an orphan is
# reparented to launchd. The command is therefore started as the leader of a
# new session: every descendant inherits that session unless it calls setsid
# itself, and no unrelated process can join it. A process is owned when it is
# either
#
# * a child of an owned process whose birth identity still matches (seen while
#   tracking, so a descendant that later calls setsid stays owned), or
# * a member of the command's session that belongs to this user and was born
#   no earlier than the command was spawned.
#
# Identities are libproc start times. A signal is sent only after the target's
# identity has been read again; without pidfds there is a short window between
# that check and kill(2), which macOS's sequential pid allocation keeps far
# from reuse. A descendant that becomes an orphan and calls setsid before the
# supervisor first sees it escapes both rules: as on Linux, logical ownership
# does not replace the runner's own teardown.

_MAC_PROC_PIDTBSDINFO = 3
_MAC_SZOMB = 5
# TERM, then KILL (getattr keeps the module importable on Windows).
_MAC_CLEANUP_SIGNALS = (signal.SIGTERM, getattr(signal, "SIGKILL", 9))


class _MacProcBsdInfo(ctypes.Structure):
    _fields_ = [
        ("pbi_flags", ctypes.c_uint32),
        ("pbi_status", ctypes.c_uint32),
        ("pbi_xstatus", ctypes.c_uint32),
        ("pbi_pid", ctypes.c_uint32),
        ("pbi_ppid", ctypes.c_uint32),
        ("pbi_uid", ctypes.c_uint32),
        ("pbi_gid", ctypes.c_uint32),
        ("pbi_ruid", ctypes.c_uint32),
        ("pbi_rgid", ctypes.c_uint32),
        ("pbi_svuid", ctypes.c_uint32),
        ("pbi_svgid", ctypes.c_uint32),
        ("rfu_1", ctypes.c_uint32),
        ("pbi_comm", ctypes.c_char * 16),
        ("pbi_name", ctypes.c_char * 32),
        ("pbi_nfiles", ctypes.c_uint32),
        ("pbi_pgid", ctypes.c_uint32),
        ("pbi_pjobc", ctypes.c_uint32),
        ("e_tdev", ctypes.c_uint32),
        ("e_tpgid", ctypes.c_uint32),
        ("pbi_nice", ctypes.c_int32),
        ("pbi_start_tvsec", ctypes.c_uint64),
        ("pbi_start_tvusec", ctypes.c_uint64),
    ]


_mac_libproc_handle = None


def _mac_libproc():
    global _mac_libproc_handle
    if sys.platform != "darwin":
        raise OSError("macOS session supervision requires macOS")
    if ctypes.sizeof(_MacProcBsdInfo) != 136:
        raise OSError("unexpected proc_bsdinfo layout")
    if _mac_libproc_handle is None:
        lib = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
        lib.proc_listallpids.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.proc_listallpids.restype = ctypes.c_int
        lib.proc_pidinfo.argtypes = [
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_uint64,
            ctypes.c_void_p,
            ctypes.c_int,
        ]
        lib.proc_pidinfo.restype = ctypes.c_int
        _mac_libproc_handle = lib
    return _mac_libproc_handle


def _mac_bsdinfo(pid):
    """Return process info, None only for ESRCH; other uncertainty raises."""
    info = _MacProcBsdInfo()
    ctypes.set_errno(0)
    size = _mac_libproc().proc_pidinfo(
        pid,
        _MAC_PROC_PIDTBSDINFO,
        0,
        ctypes.byref(info),
        ctypes.sizeof(info),
    )
    if size == 0 and ctypes.get_errno() == errno.ESRCH:
        return None
    if size != ctypes.sizeof(info) or info.pbi_pid != pid:
        # Missing permission or malformed ABI data is not evidence of exit.
        raise OSError(ctypes.get_errno() or errno.EIO,
                      f'proc_pidinfo could not verify process {pid}')
    return (
        info.pbi_ppid,
        info.pbi_uid,
        info.pbi_status,
        (info.pbi_start_tvsec, info.pbi_start_tvusec),
    )


def _mac_all_pids():
    lib = _mac_libproc()
    count = lib.proc_listallpids(None, 0)
    if count <= 0:
        raise OSError(ctypes.get_errno(), "proc_listallpids failed")
    for _ in range(4):
        capacity = count + 256
        buffer = (ctypes.c_int * capacity)()
        count = lib.proc_listallpids(buffer, ctypes.sizeof(buffer))
        if count <= 0:
            raise OSError(ctypes.get_errno(), "proc_listallpids failed")
        if count < capacity:
            return [pid for pid in buffer[:count] if pid > 0]
    raise OSError("the process list kept growing")


class MacosProcessApi:
    """macOS primitives for the owned-process supervisor (libproc, getsid)."""

    def __init__(self):
        _mac_libproc()

    def check(self):
        if self.identity(os.getpid()) is None:
            raise OSError("libproc cannot describe this process")

    def now(self):
        seconds = time.time()
        return (int(seconds), int((seconds % 1) * 1_000_000))

    def spawn(self, argv):
        return subprocess.Popen(
            argv,
            env=child_environment(),
            start_new_session=True,
        )

    def process_table(self):
        table = {}
        for pid in _mac_all_pids():
            info = _mac_bsdinfo(pid)
            if info is not None:
                table[pid] = info
        return table

    def identity(self, pid):
        info = _mac_bsdinfo(pid)
        return None if info is None else info[3]

    def session(self, pid):
        try:
            return os.getsid(pid)
        except ProcessLookupError:
            return None

    def is_alive(self, pid, identity=None):
        info = _mac_bsdinfo(pid)
        if info is None or info[2] == _MAC_SZOMB:
            return False
        return identity is None or info[3] == identity

    def terminate(self, pid, identity, sig):
        """Signal `pid` if it is still the process with `identity`."""
        info = _mac_bsdinfo(pid)
        if info is None or info[2] == _MAC_SZOMB or info[3] != identity:
            return
        try:
            os.kill(pid, sig)
        except ProcessLookupError:
            pass


class MacosSessionSupervisor:
    """Supervise verified descendants; fail closed on uncertain session ownership."""

    def __init__(self, api=None, cleanup_interval=2.0):
        self.api = api if api is not None else MacosProcessApi()
        self.cleanup_interval = cleanup_interval
        self.process = None
        self.owned = {}
        self.session = None
        self.born_after = None
        self.session_members = {}
        self.session_error = None
        self.uid = os.getuid() if hasattr(os, "getuid") else None
        self.prepared = False

    def prepare(self):
        self.api.check()
        self.prepared = True

    def spawn(self, argv):
        # The bound for session members is the leader's own start time, read
        # from the same kernel clock as theirs. Only if the leader is gone
        # before it can be read does the wall clock stand in, with a second of
        # slack for rounding.
        now = self.api.now()
        self.process = self.api.spawn(argv)
        self.session = self.process.pid
        identity = self.api.identity(self.process.pid)
        if identity is not None:
            self.owned[self.process.pid] = identity
            self.session_members[self.process.pid] = identity
            self.born_after = identity
        else:
            self.born_after = (now[0] - 1, now[1])
        return self.process

    def track(self):
        self._collect()
        if self.session_error is not None:
            raise OSError(self.session_error)
        return self.owned

    def _collect(self, table=None):
        table = self.api.process_table() if table is None else table
        self_pid = os.getpid()

        def known(pid, identity):
            # A dead owned pid that now names a new process is judged afresh.
            return self.owned.get(pid) == identity

        changed = True
        while changed:
            changed = False
            for pid, (parent, _uid, _status, identity) in table.items():
                if pid == self_pid or known(pid, identity):
                    continue
                parent_info = table.get(parent)
                if (
                    parent in self.owned
                    and parent_info is not None
                    and parent_info[3] == self.owned[parent]
                ):
                    self.owned[pid] = identity
                    changed = True
        if self.session is not None:
            members = {}
            for pid, info in table.items():
                if pid == self_pid or info[3] < self.born_after:
                    continue
                if self.api.session(pid) == self.session:
                    members[pid] = info
            # A numeric SID and a later birth time cannot prove generation.
            # At least one previously verified member must bridge snapshots.
            # Include an unreaped zombie leader: its PID cannot yet be reused.
            anchored = any(
                self.session_members.get(pid) == info[3]
                for pid, info in members.items()
            )
            if members and not anchored:
                self.session_error = "macOS session generation continuity lost"
                self.session = None
                self.session_members = {}
            elif anchored:
                self.session_members = {
                    pid: info[3] for pid, info in members.items()
                }
                for pid, (_parent, uid, _status, identity) in members.items():
                    if uid == self.uid and not known(pid, identity):
                        self.owned[pid] = identity
            if (
                not any(info[2] != _MAC_SZOMB for info in members.values())
                and self.process is not None
                and self.process.returncode is not None
            ):
                self.session = None
                self.session_members = {}
        self.owned.pop(self_pid, None)
        return self.owned

    def _alive_owned(self):
        self._collect()
        return {
            pid: identity
            for pid, identity in self.owned.items()
            if self.api.is_alive(pid, identity)
        }

    def cleanup(self):
        errors = []

        def record(error):
            message = _bounded_error(error)
            if message not in errors and len(errors) < 4:
                errors.append(message)

        if self.process is None:
            return True, errors
        if not self.prepared:
            record(OSError("the macOS supervisor was not prepared"))
            return False, errors
        remaining = dict(self.owned)
        for sig in _MAC_CLEANUP_SIGNALS:
            deadline = time.monotonic() + self.cleanup_interval
            while True:
                try:
                    alive = self._alive_owned()
                except OSError as error:
                    record(error)
                    alive = {}
                try:
                    if self.process.poll() is None:
                        self.process.send_signal(sig)
                except OSError as error:
                    record(error)
                for pid in sorted(alive):
                    if pid == self.process.pid:
                        continue
                    try:
                        self.api.terminate(pid, alive[pid], sig)
                    except OSError as error:
                        record(error)
                try:
                    self.process.poll()
                except OSError as error:
                    record(error)
                try:
                    remaining = self._alive_owned()
                except OSError as error:
                    record(error)
                    remaining = dict(self.owned)
                if not remaining and self.process.returncode is not None:
                    break
                if time.monotonic() >= deadline:
                    break
                time.sleep(0.02)
            if not remaining and self.process.returncode is not None:
                break
        try:
            self.process.wait(timeout=1)
        except (OSError, subprocess.TimeoutExpired) as error:
            record(error)
        try:
            complete = not self._alive_owned() and self.process.returncode is not None
        except OSError as error:
            record(error)
            complete = False
        if self.session_error is not None:
            record(OSError(self.session_error))
        return complete and not errors, errors


def supervisor_kind(name=None, platform_name=None):
    if (name or os.name) == "nt":
        return "windows"
    return "macos" if (platform_name or sys.platform) == "darwin" else "linux"


def owned_supervisor():
    kind = supervisor_kind()
    if kind == "windows":
        return WindowsJobSupervisor()
    if kind == "macos":
        return MacosSessionSupervisor()
    return PosixSubreaperSupervisor()


def supervisor_verdict(status):
    if status["cleanupErrors"] or status["ownedCleanupComplete"] is not True:
        return "FAIL"
    if status["error"] is not None or type(status["exitCode"]) is not int:
        return "FAIL"
    return "PASS" if status["exitCode"] == 0 else "FAIL"


def supervisor_failure(status):
    if status["cleanupErrors"] or status["ownedCleanupComplete"] is not True:
        return (
            f"{status['error'] or 'source cleanup failed'}; "
            f"cleanup: {status['cleanupErrors']}"
        )
    return status["error"]


def _install_stop_handlers(stop):
    for name in ("SIGTERM", "SIGINT"):
        try:
            signal.signal(getattr(signal, name), stop)
        except (AttributeError, OSError, ValueError):
            continue


def run_owned_supervisor(timeout, argv):
    result = {
        "exitCode": None,
        "error": None,
        "cleanupErrors": [],
        "ownedCleanupComplete": False,
    }
    supervisor = None
    interrupted = False

    def stop(_signum, _frame):
        nonlocal interrupted
        interrupted = True

    try:
        if not math.isfinite(timeout) or timeout <= 0 or not argv:
            raise ValueError("invalid source command deadline")
        supervisor = owned_supervisor()
        supervisor.prepare()
        _install_stop_handlers(stop)
        process = supervisor.spawn(argv)
        deadline = time.monotonic() + timeout
        while True:
            supervisor.track()
            if interrupted:
                raise ValueError("source supervisor interrupted")
            if time.monotonic() >= deadline:
                raise ValueError("source command timed out")
            if process.poll() is not None:
                break
            time.sleep(0.02)
        result["exitCode"] = process.returncode
    except (
        OSError,
        ValueError,
        subprocess.SubprocessError,
        KeyboardInterrupt,
    ) as error:
        result["error"] = _bounded_error(error)
    finally:
        if supervisor is not None and supervisor.process is not None:
            try:
                complete, errors = supervisor.cleanup()
                result["ownedCleanupComplete"] = complete
                result["cleanupErrors"] = errors
            except (
                OSError,
                ValueError,
                subprocess.SubprocessError,
                KeyboardInterrupt,
            ) as error:
                result["cleanupErrors"].append(_bounded_error(error))
        else:
            result["ownedCleanupComplete"] = True
        if interrupted:
            result["error"] = result["error"] or "source supervisor interrupted"
    result["verdict"] = supervisor_verdict(result)
    return result


def _execute_owned(control, timeout, argv):
    result = run_owned_supervisor(timeout, argv)
    if isinstance(control, int):
        stream = os.fdopen(control, "w", encoding="utf-8")
    else:
        stream = Path(control).open("w", encoding="utf-8")
    with stream:
        json.dump(result, stream, ensure_ascii=False)


def execute(args, log, repo, timeout=180):
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("invalid source command deadline")
    with log.open("xb") as stream:
        read_fd, write_fd = os.pipe()
        try:
            child = subprocess.Popen(
                [
                    sys.executable,
                    "-I",
                    "-B",
                    str(Path(__file__).resolve()),
                    "_owned",
                    str(write_fd),
                    str(timeout),
                    *args,
                ],
                cwd=repo,
                env=child_environment(),
                stdout=stream,
                stderr=stream,
                start_new_session=True,
                pass_fds=(write_fd,),
            )
        except BaseException:
            os.close(read_fd)
            raise
        finally:
            os.close(write_fd)
        with os.fdopen(read_fd, "rb") as control:
            try:
                child.wait(timeout=timeout + 8)
            except BaseException:
                child.terminate()
                try:
                    child.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=2)
                raise
            result = control.read(4097)
        if child.returncode != 0 or not result or len(result) > 4096:
            raise ValueError("source supervisor did not complete")
        status = json.loads(result)
        if not isinstance(status, dict) or set(status) != {
            "exitCode",
            "error",
            "cleanupErrors",
            "ownedCleanupComplete",
            "verdict",
        }:
            raise ValueError("invalid source supervisor result")
        if (
            not isinstance(status["cleanupErrors"], list)
            or len(status["cleanupErrors"]) > 4
            or not all(isinstance(error, str) for error in status["cleanupErrors"])
            or type(status["ownedCleanupComplete"]) is not bool
            or status["error"] is not None
            and not isinstance(status["error"], str)
            or status["verdict"] not in {"PASS", "FAIL"}
            or status["verdict"] != supervisor_verdict(status)
        ):
            raise ValueError("untyped source supervisor result")
        failure = supervisor_failure(status)
        if failure is not None:
            raise ValueError(failure)
        if type(status["exitCode"]) is not int:
            raise ValueError("source supervisor lacks a command result")
        return status["exitCode"]


def run_source(output, expected_head, local=False):
    output.mkdir(parents=True, exist_ok=False)
    plan_path = ROOT / "tools/qa3/source-plan.json"
    coverage_path = ROOT / "tools/qa3/coverage.json"
    receipt = {
        "schemaVersion": 1,
        "stage": "shadow",
        "sourceHead": None,
        "expectedSourceHead": expected_head,
        "workingTreeClean": None,
        "localExecution": local,
        "workflowRun": os.environ.get("GITHUB_RUN_ID"),
        "workflowAttempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
        "host": {"system": platform.system(), "architecture": platform.machine()},
        "tools": {"python": platform.python_version()},
        "sourceVerdict": "INCOMPLETE",
        "productVerdict": "UNVERIFIED",
        "runtimeQualification": "UNVERIFIED",
        "officialFirefoxMatch": "unverified",
        "publicationAuthorized": False,
        "steps": [],
    }
    try:
        if not re.fullmatch(r"[0-9a-f]{40}", expected_head):
            raise ValueError("expected source must be a full Git SHA")
        if sys.platform != "linux" or platform.machine() != "x86_64":
            raise ValueError(
                "this source host plan requires a disposable Linux x86_64 worker"
            )
        actual_head = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
        receipt["sourceHead"] = actual_head
        if actual_head != expected_head:
            raise ValueError("source checkout mismatch")
        dirty = bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT
            )
        )
        receipt["workingTreeClean"] = not dirty
        if dirty and not local:
            raise ValueError("source checkout contains uncommitted changes")
        receipt["planSha256"] = digest(plan_path.read_bytes())
        receipt["coverageSha256"] = digest(coverage_path.read_bytes())
        plan = validate_plan(read_json(plan_path))
        audit_deno_subset(ROOT)
        receipt["coverage"] = validate_coverage(read_json(coverage_path))
        receipt["plannedCases"] = sum(len(g["cases"]) for g in plan["groups"])
        receipt["testFiles"] = {
            p: digest((ROOT / p).read_bytes())
            for paths in GROUPS.values()
            for p in paths
        }
        receipt["denoLockSha256"] = digest((ROOT / "deno.lock").read_bytes())
        host_map = ROOT / "tools/qa3/deno-host-imports.json"
        host_lock = ROOT / "tools/qa3/deno-host.lock"
        validate_deno_inputs(
            read_json(host_map),
            read_json(host_lock),
            read_json(ROOT / "deno.json"),
            read_json(ROOT / "deno.lock"),
        )
        receipt["denoConfigSha256"] = digest((ROOT / "deno.json").read_bytes())
        receipt["denoHostImportMapSha256"] = digest(host_map.read_bytes())
        receipt["denoHostLockSha256"] = digest(host_lock.read_bytes())
        for tool in ("deno", "node"):
            log = output / f"{tool}-version.log"
            try:
                code = execute([tool, "--version"], log, ROOT, timeout=10)
                receipt["tools"][tool] = {
                    "exitCode": code,
                    "version": log.read_text()[:4096].strip(),
                }
            except (ValueError, OSError) as error:
                receipt["tools"][tool] = {"verdict": "UNVERIFIED", "error": str(error)}
        for group in plan["groups"]:
            record = {"id": group["id"], "verdict": "INCOMPLETE", "cases": []}
            receipt["steps"].append(record)
            report = output / f"{group['id']}.xml"
            args = {
                "deno-host": [
                    "deno",
                    "test",
                    "--no-config",
                    "--import-map",
                    str(host_map),
                    "--lock",
                    str(host_lock),
                    "--no-npm",
                    "--frozen-lockfile",
                    "--node-modules-dir=none",
                    "--no-prompt",
                    "--allow-read",
                    "--allow-write",
                    "--retry=0",
                    "--junit-path",
                    str(report),
                    *GROUPS["deno-host"],
                ],
                "node-intake": [
                    "node",
                    "--test",
                    "--test-reporter=junit",
                    *GROUPS["node-intake"],
                ],
                "python-provenance": [
                    sys.executable,
                    "-B",
                    str(Path(__file__).resolve()),
                    "python-tests",
                    str(report),
                ],
            }[group["id"]]
            log = output / f"{group['id']}.log"
            try:
                code = execute(args, log, ROOT)
                record["exitCode"] = code
                if group["id"] == "node-intake":
                    record["cases"] = read_junit(log, ROOT, GROUPS["node-intake"][0])
                elif group["id"] == "python-provenance":
                    record["cases"] = read_json(report)
                else:
                    record["cases"] = read_junit(report, ROOT)
                validate_cases(group, record["cases"])
                if code != 0:
                    raise ValueError("source command failed")
                record["verdict"] = "PASS"
            except (ValueError, TypeError, OSError, ET.ParseError) as error:
                record["error"] = str(error)
                record["verdict"] = "FAIL"
        receipt["sourceVerdict"] = (
            "PASS" if all(s["verdict"] == "PASS" for s in receipt["steps"]) else "FAIL"
        )
    except (ValueError, TypeError, OSError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
        receipt["sourceVerdict"] = "FAIL"
    finally:
        receipt["executedCases"] = sum(len(s["cases"]) for s in receipt["steps"])
        receipt["runnerSha256"] = digest(Path(__file__).read_bytes())
        (output / "source-receipt.json").write_text(
            json.dumps(receipt, indent=2) + "\n"
        )
        summary = {
            "sourceHead": receipt["sourceHead"],
            "sourceVerdict": receipt["sourceVerdict"],
            "executedCases": receipt["executedCases"],
            "publicationAuthorized": False,
            "steps": [],
        }
        if "error" in receipt:
            summary["error"] = receipt["error"]
        for step in receipt["steps"]:
            item = {key: value for key, value in step.items() if key != "cases"}
            item["executedCases"] = len(step["cases"])
            if step["verdict"] != "PASS":
                log = output / f"{step['id']}.log"
                if log.is_file():
                    with log.open("rb") as stream:
                        stream.seek(max(0, log.stat().st_size - 4096))
                        item["logTail"] = stream.read(4096).decode(
                            "utf-8", errors="replace"
                        )
            summary["steps"].append(item)
        print(json.dumps(summary))
    return 0 if receipt["sourceVerdict"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["run", "python-tests"])
    parser.add_argument("output", type=Path)
    parser.add_argument("--expected-head")
    parser.add_argument("--local", action="store_true")
    args = parser.parse_args()
    if args.command == "python-tests":
        return python_tests(args.output)
    return run_source(args.output.resolve(), args.expected_head or "", args.local)


if __name__ == "__main__":
    if len(sys.argv) >= 5 and sys.argv[1] == "_owned":
        _execute_owned(int(sys.argv[2]), float(sys.argv[3]), sys.argv[4:])
        sys.exit(0)
    sys.exit(main())
