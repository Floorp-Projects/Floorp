# SPDX-License-Identifier: MPL-2.0
"""No browser, subprocess, network or macOS dependency is needed here."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import plistlib
import tempfile
import unittest
from unittest.mock import Mock, patch

import shim_diagnostics as diagnostics


spec = importlib.util.spec_from_file_location("context_menu_probe", Path(__file__).with_name("test-context-menu.py"))
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
SECRET = "private-credential-value-0123456789"


class DiagnosticTests(unittest.TestCase):
    def test_streams_keep_only_allowlisted_markers_and_bound_the_tail(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            private = root / "private"
            private.mkdir()
            (private / "browser.log").write_text("open helper " + SECRET)
            (private / "browser.stdout.log").write_text("x" * 70000 + "\nApp Shim could not connect to its registered browser host. token=" + SECRET + "\n")
            (private / "browser.stderr.log").write_text("Authorization: Bearer " + SECRET + "\nNS_ERROR_NOT_AVAILABLE nonce=" + SECRET)
            result = diagnostics.save_streams(private, root)
            self.assertTrue(result["browser.stdout.log"]["tailTruncated"])
            self.assertLessEqual(result["browser.stdout.log"]["tailBytesRead"], 65536)
            self.assertEqual(result["browser.stdout.log"]["markers"], ["shim-host-lookup-failed"])
            self.assertEqual(result["browser.stderr.log"]["codes"], ["NS_ERROR_NOT_AVAILABLE"])
            for name in result:
                self.assertNotIn(SECRET, (root / name).read_text())
                self.assertNotIn("Bearer", (root / name).read_text())

    def test_missing_streams_are_explicit_and_not_fabricated_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = diagnostics.save_streams(Path(tmp) / "missing", Path(tmp))
            self.assertTrue(all(not row["present"] for row in result.values()))

    def test_one_unreadable_stream_does_not_prevent_other_stream_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            private = root / "private"
            private.mkdir()
            for name in ("browser.log", "browser.stdout.log", "browser.stderr.log"):
                (private / name).write_text("NS_ERROR_NOT_AVAILABLE " + SECRET)
            original = Path.open
            def open_path(path, *args, **kwargs):
                if path == private / "browser.stdout.log":
                    raise PermissionError(SECRET)
                return original(path, *args, **kwargs)
            with patch.object(Path, "open", open_path):
                result = diagnostics.save_streams(private, root)
            self.assertEqual(result["browser.stdout.log"]["error"]["type"], "PermissionError")
            self.assertEqual(result["browser.stderr.log"]["codes"], ["NS_ERROR_NOT_AVAILABLE"])
            self.assertNotIn(SECRET, json.dumps(result))

    def test_events_preserve_disconnect_reason_without_payload_credentials(self):
        events = [{"type":"connected","appId":SECRET,"diagnosticTime":12,
                   "payload":{"pid":432,"nonce":SECRET}},
                  {"type":"disconnected","diagnosticTime":15,
                   "payload":{"reason":"process-exited","launchToken":SECRET}},
                  {"type":"disconnected","payload":{"reason":SECRET}},
                  {"type":100,"payload":{"text":SECRET}}]
        result = diagnostics.event_summary(events)
        self.assertEqual(result["events"][0], {"type":"connected","timeMs":12,"pid":432})
        self.assertEqual(result["events"][1]["reason"], "process-exited")
        self.assertEqual(result["events"][2]["reason"], "other")
        self.assertNotIn(SECRET, json.dumps(result))

    def test_event_limit_and_unknown_payload_do_not_leak(self):
        result = diagnostics.event_summary([{"type":"disconnected","payload":{"reason":[SECRET]}}] * 100)
        self.assertTrue(result["truncated"])
        self.assertEqual(len(result["events"]), 64)
        self.assertNotIn(SECRET, json.dumps(result))

    def test_authentication_event_survives_many_unrelated_input_events(self):
        result = diagnostics.event_summary([{"type":"connected","payload":{"pid":42}}]
                                          + [{"type":100,"payload":{"text":SECRET}}] * 200)
        self.assertEqual(result["events"], [{"type":"connected","pid":42}])
        self.assertFalse(result["truncated"])

    def test_error_body_and_command_arguments_are_not_published(self):
        value = diagnostics.error_summary(RuntimeError("ExecuteScript token=" + SECRET + " NS_ERROR_NOT_AVAILABLE"))
        self.assertEqual(value, {"type":"RuntimeError","codes":["NS_ERROR_NOT_AVAILABLE"]})
        self.assertNotIn(SECRET, json.dumps(value))

    def test_unknown_browser_exit_does_not_become_launcher_success(self):
        host = Mock(pid=123, verified=True, launch_services=True, requested_signal=None,
                    exited=True, returncode=None)
        host.process.poll.return_value = 0
        value = diagnostics.process_summary(host)
        self.assertIsNone(value["browserExitCode"])
        self.assertEqual(value["launcherExitCode"], 0)

    def test_first_failure_cannot_be_replaced_by_cleanup_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            diagnostics.write_first_failure(tmp, {"error":{"timeout":"Shim authentication"}})
            before = (Path(tmp) / "first-failure.json").read_bytes()
            diagnostics.write_first_failure(tmp, {"error":{"timeout":"cleanup"}})
            self.assertEqual((Path(tmp) / "first-failure.json").read_bytes(), before)


class ProbeFailureTests(unittest.TestCase):
    def run_probe(self, *, shim_cleanup_fails=False, host_cleanup_fails=False, snapshot_fails=False,
                  client_cleanup_fails=False, ownership_fails=False, monitor_close_fails=False,
                  monitor_snapshot_fails=False, lifecycle_write_fails=False, system_log_fails=False):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        browser = root / "Test.app"
        (browser / "Contents").mkdir(parents=True)
        (browser / "Contents/Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable":"floorp"}))
        host = Mock(pid=123, verified=True, launch_services=True, requested_signal=None,
                    exited=False, returncode=None)
        host.poll.return_value = None
        host.process.poll.return_value = 0
        if ownership_fails:
            host.verified = False
            host.verify_session.side_effect = RuntimeError("wrong owner " + SECRET)

        def launch(_browser, _exe, _profile, private, log, **kwargs):
            log.write("launcher credential=" + SECRET)
            (private / "browser.stdout.log").write_text("Authorization: " + SECRET)
            (private / "browser.stderr.log").write_text("App Shim refused startup: signed bundle and pinned host identity are required. nonce=" + SECRET)
            return host

        def close_host():
            if host_cleanup_fails:
                raise RuntimeError("host cleanup " + SECRET)
            host.exited = True
            host.returncode = -15
            host.requested_signal = 15
        host.close.side_effect = close_host
        client = Mock(session={})
        if client_cleanup_fails:
            client.close.side_effect = RuntimeError("client cleanup " + SECRET)

        def script(source, *args):
            if ".observer=" in source:
                self.observer_script = source
            if "hostIdentityJSON" in source:
                return "{}"
            if "sendControl" in source and shim_cleanup_fails:
                raise RuntimeError("NS_ERROR_NOT_AVAILABLE token=" + SECRET)
            if "return {events:" in source:
                output = next((root / "evidence").glob("run-*"))
                self.assertTrue((output / "first-failure.json").is_file())
                if snapshot_fails:
                    raise RuntimeError("snapshot cookie=" + SECRET)
                return {"events":[{"type":"disconnected","payload":{"reason":"launch-failed","nonce":SECRET}}], "authCount":1,
                        "crashCount":0,"failureCount":0}
            return None
        client.script.side_effect = script

        def wait_for(description, callback, timeout):
            if description == "Marionette":
                self.assertEqual(timeout, 35)
                return callback()
            self.assertEqual(description, "Shim authentication")
            self.assertEqual(timeout, 20)
            raise RuntimeError("Timed out waiting for Shim authentication")
        monitor = Mock(identity=None, pid=None, started_wall=0, status="simulated")
        monitor.snapshot.return_value = {"status":"simulated", "events":[]}
        if monitor_close_fails:
            monitor.close.side_effect = RuntimeError("monitor PRIVATE " + SECRET)
        if monitor_snapshot_fails:
            monitor.snapshot.side_effect = RuntimeError("snapshot PRIVATE " + SECRET)
        write_text_original = Path.write_text
        def write_text(path, *args, **kwargs):
            if lifecycle_write_fails and path.name == "shim-lifecycle.json":
                raise OSError("write PRIVATE " + SECRET)
            return write_text_original(path, *args, **kwargs)
        stdout = io.StringIO()
        with patch.object(Path, "write_text", new=write_text), patch.object(probe.h, "run"), patch.object(probe.h, "unused_port", return_value=12345), \
             patch.object(probe.lifecycle, "binary_summary", return_value={}), \
             patch.object(probe.lifecycle, "system_log_summary", return_value={"status":"simulated","events":[]}, side_effect=RuntimeError("PRIVATE " + SECRET) if system_log_fails else None), \
             patch.object(probe.lifecycle, "ShimMonitor", return_value=monitor), \
             patch.object(probe.h, "HostProcess", side_effect=launch), \
             patch.object(probe.h, "Marionette", return_value=client), \
             patch.object(probe.h, "seal_bundle"), patch.object(probe.h, "wait_for", side_effect=wait_for), \
             patch("sys.argv", ["probe", "--browser", str(browser), "--output", str(root / "evidence")]), \
             contextlib.redirect_stdout(stdout):
            code = probe.main()
        output = next((root / "evidence").glob("run-*"))
        report = json.loads((output / "result.json").read_text())
        first = json.loads((output / "first-failure.json").read_text())
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["phase"], "connect-marionette" if ownership_fails else "authenticate-shim")
        self.assertEqual(report["attemptedIterations"], 0)
        if not ownership_fails:
            self.assertEqual(first["error"]["timeout"], "Shim authentication")
        self.assertNotIn("cleanupErrors", first)
        self.assertNotIn(SECRET, stdout.getvalue())
        for file in output.iterdir():
            if file.is_file():
                self.assertNotIn(SECRET, file.read_text())
        host.close.assert_called_once()
        if ownership_fails:
            client.context.assert_not_called()
            client.script.assert_not_called()
            client.close.assert_not_called()
            client.socket.close.assert_called_once()
        else:
            client.close.assert_called_once()
        return report, first

    def test_authentication_timeout_retains_failure_streams_events_and_process_states(self):
        report, first = self.run_probe()
        self.assertEqual(report["beforeCleanup"]["shim"]["events"][0]["reason"], "launch-failed")
        self.assertFalse(report["beforeCleanup"]["host"]["browserExited"])
        self.assertTrue(report["afterCleanup"]["host"]["browserExited"])
        self.assertEqual(report["afterCleanup"]["host"]["browserExitCode"], -15)
        self.assertIsNone(report["beforeCleanup"]["shimExitCode"])
        self.assertEqual(report["streams"]["browser.stderr.log"]["markers"], ["shim-startup-refused"])

    def test_shim_and_host_cleanup_failures_still_write_primary_failure_and_streams(self):
        report, first = self.run_probe(shim_cleanup_fails=True, host_cleanup_fails=True)
        self.assertEqual([e["step"] for e in report["cleanupErrors"]], ["shim", "host"])
        self.assertEqual(report["error"], first["error"])
        self.assertTrue(report["streams"]["browser.stdout.log"]["present"])

    def test_disconnected_marionette_snapshot_does_not_replace_timeout(self):
        report, first = self.run_probe(snapshot_fails=True)
        self.assertIn("stateError", report["beforeCleanup"])
        self.assertEqual(report["error"], first["error"])

    def test_client_close_failure_still_saves_host_exit_and_streams(self):
        report, _ = self.run_probe(client_cleanup_fails=True)
        self.assertEqual(report["cleanupErrors"][0]["step"], "client")
        self.assertTrue(report["afterCleanup"]["host"]["browserExited"])
        self.assertTrue(report["streams"]["browser.stderr.log"]["present"])

    def test_new_monitor_failures_do_not_prevent_primary_result_or_streams(self):
        for options in [{"monitor_close_fails":True}, {"monitor_snapshot_fails":True}, {"lifecycle_write_fails":True}, {"system_log_fails":True}]:
            report, first = self.run_probe(**options)
            self.assertEqual(report["error"], first["error"])
            self.assertTrue(report["diagnosticsIncomplete"])
            self.assertTrue(report["streams"]["browser.stderr.log"]["present"])

    def test_wrong_process_ownership_never_receives_chrome_or_cleanup_commands(self):
        report, _ = self.run_probe(ownership_fails=True)
        self.assertIn("stateUnavailable", report["beforeCleanup"])


if __name__ == "__main__":
    unittest.main()
