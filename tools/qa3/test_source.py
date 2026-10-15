# SPDX-License-Identifier: MPL-2.0

import copy
import ctypes
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import source


class SourceContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan = source.read_json(source.ROOT / "tools/qa3/source-plan.json")
        self.coverage = source.read_json(source.ROOT / "tools/qa3/coverage.json")

    def test_fixed_plan_has_all_109_unique_host_cases(self):
        source.validate_plan(self.plan)
        self.assertEqual(sum(len(g["cases"]) for g in self.plan["groups"]), 109)
        source.audit_deno_subset(source.ROOT)

    def test_host_deno_inputs_match_root_aliases_and_frozen_jsr_hashes(self):
        host_map = source.read_json(source.ROOT / "tools/qa3/deno-host-imports.json")
        host_lock = source.read_json(source.ROOT / "tools/qa3/deno-host.lock")
        repo_config = source.read_json(source.ROOT / "deno.json")
        repo_lock = source.read_json(source.ROOT / "deno.lock")
        source.validate_deno_inputs(host_map, host_lock, repo_config, repo_lock)
        for field, value in (
            ("imports", {**host_map["imports"], "extra": "npm:unrelated"}),
            ("imports", {**host_map["imports"], "@std/assert": "jsr:@std/assert@0"}),
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                source.validate_deno_inputs(
                    {**host_map, field: value}, host_lock, repo_config, repo_lock
                )
        changed = copy.deepcopy(host_lock)
        first_package = next(iter(changed["jsr"]))
        changed["jsr"][first_package]["integrity"] = "0" * 64
        for lock in (
            {**host_lock, "npm": {}},
            {**host_lock, "workspace": repo_lock["workspace"]},
            {**host_lock, "specifiers": {}},
            changed,
        ):
            with self.subTest(lock=lock), self.assertRaises(ValueError):
                source.validate_deno_inputs(host_map, lock, repo_config, repo_lock)
        for config in (
            {},
            {"imports": {**repo_config["imports"], "@std/assert": "npm:assert"}},
        ):
            with self.subTest(config=config), self.assertRaises(ValueError):
                source.validate_deno_inputs(host_map, host_lock, config, repo_lock)

    def test_missing_empty_reordered_and_duplicate_groups_are_rejected(self):
        changes = [
            [],
            self.plan["groups"][:-1],
            self.plan["groups"][::-1],
            self.plan["groups"] * 2,
        ]
        for groups in changes:
            with self.subTest(groups=groups), self.assertRaises(ValueError):
                source.validate_plan({**self.plan, "groups": groups})

    def test_browser_tasks_arbitrary_arguments_and_foreign_origin_are_rejected(self):
        for key, value in (
            ("origin", "mozilla"),
            ("executionKind", "browser"),
            ("stage", "required"),
            ("target", "mac-aarch64"),
        ):
            with self.subTest(key=key), self.assertRaises(ValueError):
                source.validate_plan({**self.plan, key: value})
        for field, value in (
            ("paths", ["tools/src/colocated_test_runner.ts"]),
            ("argv", ["deno", "task", "test:integration"]),
        ):
            changed = copy.deepcopy(self.plan)
            changed["groups"][0][field] = value
            with self.assertRaises(ValueError):
                source.validate_plan(changed)

    def test_reduced_or_duplicated_test_ids_are_rejected(self):
        for transform in (lambda c: c[:-1], lambda c: c[:1] * len(c), lambda c: []):
            changed = copy.deepcopy(self.plan)
            changed["groups"][0]["cases"] = transform(changed["groups"][0]["cases"])
            with self.assertRaises(ValueError):
                source.validate_plan(changed)

    def test_unexecuted_duplicate_unknown_failed_and_skipped_cases_cannot_pass(self):
        group = self.plan["groups"][0]
        actual = [{**c, "verdict": "PASS"} for c in group["cases"]]
        source.validate_cases(group, actual)
        changes = [
            actual[:-1],
            actual + actual[:1],
            [{**actual[0], "name": "unknown"}, *actual[1:]],
        ]
        for verdict in ("FAIL", "SKIP", "INCOMPLETE", "RECOVERED"):
            changes.append([{**actual[0], "verdict": verdict}, *actual[1:]])
        for changed in changes:
            with self.subTest(changed=changed[0]), self.assertRaises(ValueError):
                source.validate_cases(group, changed)

    def test_junit_checks_case_identity_and_completion(self):
        report = self.root / "report.xml"
        report.write_text(
            '<testsuites tests="1" failures="0"><testsuite><testcase classname="./tools/src/utils.test.ts" name="one"/></testsuite><!-- pass 1 --></testsuites>'
        )
        self.assertEqual(
            source.read_junit(report, self.root),
            [{"file": "tools/src/utils.test.ts", "name": "one", "verdict": "PASS"}],
        )
        report.write_text(
            '<testsuite><testcase file="/foreign/source.ts" name="one"/></testsuite>'
        )
        with self.assertRaises(ValueError):
            source.read_junit(report, self.root)

    def test_junit_rejects_entities_skip_failure_and_aggregate_contradictions(self):
        report = self.root / "report.xml"
        for text in (
            '<!DOCTYPE x [<!ENTITY y "hidden">]><testsuite/>',
            '<testsuite tests="2"><testcase name="one"/></testsuite>',
            '<testsuite failures="1"><testcase name="one"/></testsuite>',
            '<testsuite skipped="1"><testcase name="one"/></testsuite>',
        ):
            report.write_text(text)
            with self.assertRaises(ValueError):
                source.read_junit(report, self.root)
        for child in ("failure", "error", "skipped", "flakyFailure"):
            report.write_text(
                f'<testsuite><testcase name="one" classname="file"><{child}/></testcase></testsuite>'
            )
            self.assertEqual(source.read_junit(report, self.root)[0]["verdict"], "FAIL")

    def test_junit_requires_utf8_and_rejects_encoded_dtd_entities(self):
        report = self.root / "encoded.xml"
        document = '<testsuite><testcase classname="file" name="日本語"/></testsuite>'
        unsafe = '<!DOCTYPE testsuite [<!ENTITY origin "file">]><testsuite><testcase classname="&origin;" name="one"/></testsuite>'
        for encoding in (
            "utf-16",
            "utf-16-le",
            "utf-16-be",
            "utf-32",
            "utf-32-le",
            "utf-32-be",
        ):
            for text in (document, unsafe):
                with self.subTest(encoding=encoding, unsafe=text == unsafe):
                    report.write_bytes(text.encode(encoding))
                    with self.assertRaises(ValueError):
                        source.read_junit(report, self.root)
        for encoding in ("utf-8", "utf-8-sig"):
            report.write_bytes(document.encode(encoding))
            self.assertEqual(source.read_junit(report, self.root)[0]["name"], "日本語")
            report.write_bytes(unsafe.encode(encoding))
            with self.assertRaises(ValueError):
                source.read_junit(report, self.root)
        report.write_bytes(b'<?xml version="1.0" encoding="ISO-8859-1"?><testsuite/>')
        with self.assertRaises(ValueError):
            source.read_junit(report, self.root)

    def test_node22_classname_is_bound_only_to_the_fixed_single_file_command(self):
        report = self.root / "node22.xml"
        report.write_text(
            '<testsuites><testcase classname="test" name="one"/></testsuites>'
        )
        file = source.GROUPS["node-intake"][0]
        self.assertEqual(source.read_junit(report, self.root, file)[0]["file"], file)
        self.assertEqual(source.read_junit(report, self.root)[0]["file"], "test")
        with self.assertRaises(ValueError):
            source.read_junit(report, self.root, "foreign-test.mjs")
        report.write_text(
            '<testsuites><testcase file="" classname="test" name="one"/></testsuites>'
        )
        self.assertEqual(source.read_junit(report, self.root, file)[0]["file"], "test")
        report.write_text(
            '<testsuites><testcase file="/foreign/test.mjs" classname="test" name="one"/></testsuites>'
        )
        with self.assertRaises(ValueError):
            source.read_junit(report, self.root, file)

    def test_per_test_retry_ignore_and_options_are_outside_the_fixed_host_plan(self):
        for path in source.GROUPS["deno-host"]:
            file = self.root / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('Deno.test("one", () => {});')
        source.audit_deno_subset(self.root)
        file.write_text('Deno.test({name: "one", retry: 2, fn: () => {}});')
        with self.assertRaises(ValueError):
            source.audit_deno_subset(self.root)
        file.write_text('Deno.test("one", {ignore: true}, () => {});')
        with self.assertRaises(ValueError):
            source.audit_deno_subset(self.root)

    def test_all_publication_kinds_and_five_native_targets_remain_unverified(self):
        source.validate_coverage(self.coverage)
        for kind in source.ARTIFACT_TARGETS:
            changed = copy.deepcopy(self.coverage)
            changed["artifacts"] = [
                e for e in changed["artifacts"] if e["kind"] != kind
            ]
            with self.assertRaises(ValueError):
                source.validate_coverage(changed)
        for value in (True, 0, "false"):
            with self.assertRaises(ValueError):
                source.validate_coverage(
                    {
                        **self.coverage,
                        "publicationAuthorized": value,
                    }
                )
        changed = copy.deepcopy(self.coverage)
        changed["artifacts"][0]["verdict"] = "PASS"
        with self.assertRaises(ValueError):
            source.validate_coverage(changed)
        changed["artifacts"][0]["verdict"] = "UNVERIFIED"
        changed["artifacts"][0]["targets"].pop()
        with self.assertRaises(ValueError):
            source.validate_coverage(changed)

    def test_policy_json_rejects_duplicate_fields_and_boolean_schema(self):
        file = self.root / "plan.json"
        file.write_text('{"stage":"shadow","stage":"required"}')
        with self.assertRaises(ValueError):
            source.read_json(file)
        with self.assertRaises(ValueError):
            source.validate_plan({**self.plan, "schemaVersion": True})

    def test_children_do_not_inherit_credentials_or_runner_options(self):
        with patch.dict(
            os.environ,
            {
                "GH_TOKEN": "fixture-secret",
                "NODE_OPTIONS": "bad",
                "DENO_FLAGS": "bad",
                "DENO_NO_PACKAGE_JSON": "0",
            },
        ):
            env = source.child_environment()
            self.assertNotIn("GH_TOKEN", env)
            self.assertNotIn("NODE_OPTIONS", env)
            self.assertNotIn("DENO_FLAGS", env)
            self.assertEqual(env["DENO_NO_PACKAGE_JSON"], "1")
        code = source.execute(
            [sys.executable, "-c", "print('bounded source fixture')"],
            self.root / "child.log",
            self.root,
        )
        self.assertEqual(code, 0)

    def test_timeout_kills_the_owned_process_group(self):
        marker = self.root / "descendant-survived"
        child = f"import time,pathlib; time.sleep(.2); pathlib.Path({str(marker)!r}).write_text('bad')"
        parent = f"import subprocess,sys,time; subprocess.Popen([sys.executable,'-c',{child!r}]); time.sleep(10)"
        with self.assertRaisesRegex(ValueError, "timed out"):
            source.execute(
                [sys.executable, "-c", parent],
                self.root / "timeout.log",
                self.root,
                timeout=0.1,
            )
        time.sleep(0.25)
        self.assertFalse(marker.exists())

    def test_exit_recovers_detached_descendant_and_preserves_unrelated_child(self):
        libc = ctypes.CDLL(None, use_errno=True)
        prior = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(prior), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)
        unrelated = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(10)"]
        )
        marker = self.root / "detached-pid"
        descendant = (
            "import os,signal,time,pathlib; "
            "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
            "birth=pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]; "
            f"pathlib.Path({str(marker)!r}).write_text(str(os.getpid())+' '+birth); "
            "time.sleep(10)"
        )
        command = (
            "import pathlib,subprocess,sys,time; "
            f"subprocess.Popen([sys.executable,'-c',{descendant!r}],start_new_session=True); "
            f"marker=pathlib.Path({str(marker)!r}); "
            "deadline=time.monotonic()+3\n"
            "while not marker.exists() and time.monotonic()<deadline: time.sleep(.01)\n"
            "assert marker.exists()"
        )
        pid = None
        try:
            self.assertEqual(
                source.execute(
                    [sys.executable, "-c", command],
                    self.root / "detached.log",
                    self.root,
                ),
                0,
            )
            pid, birth = map(int, marker.read_text().split())
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                alive = False
            else:
                alive = True
            self.assertFalse(
                alive, "detached owned descendant survived command completion"
            )
            self.assertIsNone(unrelated.poll())
        finally:
            if pid is None and marker.exists():
                pid, birth = map(int, marker.read_text().split())
            if pid is not None:
                info = source._process_info(pid)
                if info and info[2] == birth:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                try:
                    os.waitpid(pid, 0)
                except ChildProcessError:
                    pass
            unrelated.kill()
            unrelated.wait(timeout=3)
            self.assertEqual(libc.prctl(36, prior.value, 0, 0, 0), 0)

    def test_timeout_recovers_detached_child_that_forks_during_term(self):
        marker = self.root / "late-fork-pid"
        ready = self.root / "ready"
        command = f"""
import os, pathlib, signal, subprocess, sys, time
def on_term(*_):
    if os.fork() == 0:
        os.setsid()
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        birth=pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]
        pathlib.Path({str(marker)!r}).write_text(str(os.getpid())+' '+birth)
        time.sleep(10)
        os._exit(0)
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
signal.signal(signal.SIGTERM, on_term)
birth=pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]
pathlib.Path({str(ready)!r}).write_text(str(os.getpid())+' '+birth)
time.sleep(10)
"""
        try:
            with self.assertRaisesRegex(ValueError, "timed out"):
                source.execute(
                    [sys.executable, "-c", command],
                    self.root / "late-fork.log",
                    self.root,
                    timeout=0.5,
                )
            self.assertTrue(
                ready.exists(),
                "owned fixture did not become ready before its deadline; log: "
                + (self.root / "late-fork.log").read_text(),
            )
            self.assertTrue(
                marker.exists(), "the late-fork cleanup path was not exercised"
            )
            with self.assertRaises(ProcessLookupError):
                os.kill(int(marker.read_text().split()[0]), 0)
        finally:
            for file in (ready, marker):
                if file.exists():
                    pid, birth = map(int, file.read_text().split())
                    info = source._process_info(pid)
                    if info and info[2] == birth:
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass

    def test_execute_keeps_parent_subreaper_state_and_rejects_invalid_deadlines(self):
        libc = ctypes.CDLL(None, use_errno=True)
        prior = ctypes.c_int()
        after = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(prior), 0, 0, 0), 0)
        self.assertEqual(
            source.execute(
                [sys.executable, "-c", "raise SystemExit(124)"],
                self.root / "nonzero.log",
                self.root,
            ),
            124,
        )
        self.assertEqual(libc.prctl(37, ctypes.byref(after), 0, 0, 0), 0)
        self.assertEqual(after.value, prior.value)
        for timeout in (0, -1, float("nan"), float("inf")):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                source.execute(
                    [sys.executable, "-c", "pass"],
                    self.root / "invalid.log",
                    self.root,
                    timeout,
                )
        self.assertFalse((self.root / "invalid.log").exists())

    def test_spawn_failure_closes_control_pipe_without_overwriting_log(self):
        count = len(list(Path("/proc/self/fd").iterdir()))
        with (
            patch.object(
                source.subprocess, "Popen", side_effect=OSError("fixture spawn failure")
            ),
            self.assertRaisesRegex(OSError, "fixture spawn failure"),
        ):
            source.execute(
                [sys.executable, "-c", "pass"],
                self.root / "spawn-fail.log",
                self.root,
            )
        self.assertEqual(len(list(Path("/proc/self/fd").iterdir())), count)

    def test_cleanup_fault_and_interruption_are_recorded_without_leaking_children(self):
        libc = ctypes.CDLL(None, use_errno=True)
        prior = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(prior), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)
        fixture = """
import errno, os, pathlib, signal, sys
sys.path.insert(0, sys.argv[2])
import source
marker = sys.argv[3]
if sys.argv[4] == 'pidfd':
    real = source.os.pidfd_open
    first = True
    def fault(pid):
        global first
        if first:
            first = False
            raise OSError(errno.EMFILE, 'fixture transient pidfd failure')
        return real(pid)
    source.os.pidfd_open = fault
    child = "import os,pathlib,signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); birth=pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]; pathlib.Path("+repr(marker)+").write_text(str(os.getpid())+' '+birth); time.sleep(10)"
else:
    real = source._cleanup_owned
    def interrupt(process, owned):
        os.kill(os.getpid(), signal.SIGTERM)
        return real(process, owned)
    source._cleanup_owned = interrupt
    child = 'raise SystemExit(0)'
source._execute_owned(int(sys.argv[1]), .5, [sys.executable, '-I', '-c', child])
"""
        try:
            for mode in ("pidfd", "interrupted"):
                with self.subTest(mode=mode):
                    marker = self.root / f"{mode}-child"
                    read_fd, write_fd = os.pipe()
                    worker = None
                    try:
                        worker = subprocess.Popen(
                            [
                                sys.executable,
                                "-I",
                                "-B",
                                "-c",
                                fixture,
                                str(write_fd),
                                str(Path(source.__file__).parent),
                                str(marker),
                                mode,
                            ],
                            pass_fds=(write_fd,),
                            env=source.child_environment(),
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            start_new_session=True,
                        )
                    except BaseException:
                        os.close(read_fd)
                        raise
                    finally:
                        os.close(write_fd)
                    try:
                        worker.wait(timeout=8)
                        with os.fdopen(read_fd, "rb") as control:
                            read_fd = None
                            result = json.loads(control.read(4097))
                        self.assertTrue(result["ownedCleanupComplete"])
                        if mode == "pidfd":
                            self.assertIn("timed out", result["error"])
                            self.assertIn(
                                "fixture transient pidfd failure",
                                result["cleanupErrors"][0],
                            )
                            self.assertTrue(
                                marker.exists(),
                                f"owned pidfd fixture did not become ready; control: {result}",
                            )
                            with self.assertRaises(ProcessLookupError):
                                os.kill(int(marker.read_text().split()[0]), 0)
                        else:
                            self.assertEqual(result["exitCode"], 0)
                            self.assertIn("supervisor interrupted", result["error"])
                    finally:
                        if read_fd is not None:
                            os.close(read_fd)
                        if worker is not None and worker.poll() is None:
                            worker.kill()
                            worker.wait(timeout=2)
                        if marker.exists():
                            pid, birth = map(int, marker.read_text().split())
                            info = source._process_info(pid)
                            if info and info[2] == birth:
                                try:
                                    os.kill(pid, signal.SIGKILL)
                                except ProcessLookupError:
                                    pass
                            try:
                                os.waitpid(pid, 0)
                            except ChildProcessError:
                                pass
        finally:
            self.assertEqual(libc.prctl(36, prior.value, 0, 0, 0), 0)

    def test_reusing_an_output_log_cannot_erase_a_first_failure(self):
        log = self.root / "first.log"
        log.write_text("first FAIL\n")
        with self.assertRaises(FileExistsError):
            source.execute([sys.executable, "-c", "print('PASS')"], log, self.root)
        self.assertEqual(log.read_text(), "first FAIL\n")

    def test_bad_source_or_host_still_preserves_a_fail_receipt(self):
        head = source.subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=source.ROOT, text=True
        ).strip()
        for name, expected in (("missing", ""), ("mismatch", "0" * 40)):
            output = self.root / name
            self.assertEqual(source.run_source(output, expected), 1)
            receipt = source.read_json(output / "source-receipt.json")
            self.assertEqual(receipt["sourceVerdict"], "FAIL")
            self.assertEqual(receipt["executedCases"], 0)
            self.assertIs(receipt["publicationAuthorized"], False)
        with patch.object(source.platform, "machine", return_value="aarch64"):
            output = self.root / "foreign-host"
            self.assertEqual(source.run_source(output, head), 1)
            self.assertIn(
                "Linux x86_64",
                source.read_json(output / "source-receipt.json")["error"],
            )


if __name__ == "__main__":
    unittest.main()
