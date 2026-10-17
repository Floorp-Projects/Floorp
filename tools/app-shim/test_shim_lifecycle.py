# SPDX-License-Identifier: MPL-2.0
import unittest
import ctypes
import datetime
import json
import os
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
from unittest.mock import Mock, patch
import shim_lifecycle as lifecycle
import shim_diagnostics as diagnostics


class NativeDiagnosticRegression(unittest.TestCase):
    def test_preserves_cause_before_terminal_reason(self):
        report = diagnostics.event_summary([{
            "type": "disconnected", "diagnosticTime": 1234,
            "payload": {"reason": "process-exited", "causeReason": "send-failed",
                        "causeTimeMs": 1200, "connectedBeforeDisconnect": True,
                        "pid": 123, "launchToken": "PRIVATE"}}])
        event = report["events"][0]
        self.assertEqual(event["reason"], "process-exited")
        self.assertEqual(event["causeReason"], "send-failed")
        self.assertEqual(event["causeTimeMs"], 1200)
        self.assertIs(event["connectedBeforeDisconnect"], True)
        self.assertNotIn("PRIVATE", str(report))

    def test_no_connected_event_does_not_establish_preauth_exit(self):
        report = diagnostics.event_summary([{
            "type": "disconnected", "diagnosticTime": 100,
            "payload": {"reason": "process-exited"}}])
        self.assertEqual(report["authenticationState"], "connection-not-observed")
        self.assertIsNone(report["nativeAuthenticationCompleted"])

    def test_observed_connection_order_is_retained(self):
        report = diagnostics.event_summary([
            {"type":"connected","sequence":1,"diagnosticTime":20,"payload":{"pid":42}},
            {"type":"disconnected","sequence":2,"diagnosticTime":21,"payload":{"reason":"process-exited"}}])
        self.assertEqual([e["sequence"] for e in report["events"]], [1,2])
        self.assertEqual(report["authenticationState"], "connection-observed")


class ExitObservation(unittest.TestCase):
    def test_exit_status_absence_never_becomes_success(self):
        self.assertEqual(lifecycle.decode_exit(0, 0),
                         {"exitCode":None,"signal":None,"exitStatusAvailable":False})
        self.assertEqual(lifecycle.decode_exit(1 << 8, lifecycle.EXITSTATUS)["exitCode"], 1)
        value=lifecycle.decode_exit(9 | 0x40000, lifecycle.EXITSTATUS | lifecycle.EXIT_DETAIL)
        self.assertEqual(value["signal"],9)
        self.assertTrue(value["codeSigningKill"])

    def run_observer(self, identities, denied=False):
        monitor=lifecycle.ShimMonitor("/private/unique/Shim",123,Mock())
        monitor._identity=Mock(side_effect=identities)
        queue=Mock()
        event=SimpleNamespace(ident=42,fflags=0x80000000 | lifecycle.EXITSTATUS,data=256)
        queue.control.side_effect=PermissionError(13,"PRIVATE") if denied else [[],[event]]
        fake_select=SimpleNamespace(kqueue=Mock(return_value=queue),kevent=Mock(),
            KQ_FILTER_PROC=1,KQ_EV_ADD=1,KQ_EV_ONESHOT=2,KQ_NOTE_EXIT=0x80000000)
        with patch.object(lifecycle,"select",fake_select), patch.object(lifecycle.subprocess,"run",return_value=SimpleNamespace(stdout="42")):
            monitor._observe()
        queue.close.assert_called_once()
        return monitor.snapshot()

    def test_owned_pid_exit_is_timestamped_separately(self):
        value=self.run_observer([(42,1,2),(42,1,2)])
        self.assertEqual(value["status"],"exit-observed")
        self.assertEqual(value["events"][-1]["exitCode"],1)
        self.assertLessEqual(value["events"][0]["monotonicNs"],value["events"][-1]["monotonicNs"])
        self.assertIsNone(value["nativeAuthenticationCompleted"])

    def test_pid_reuse_and_permission_denial_remain_unknown(self):
        for identities,denied in [([(42,1,2),(42,1,3)],False), ([(42,1,2)],True)]:
            value=self.run_observer(identities,denied)
            self.assertEqual(value["status"],"unavailable")
            self.assertFalse(any(e["kind"]=="kernel-exit-observed" for e in value["events"]))
            self.assertNotIn("PRIVATE",json.dumps(value))

    def test_process_gone_before_registration_remains_unknown(self):
        value=self.run_observer([(42,1,2),None])
        self.assertEqual(value["status"],"unavailable")
        self.assertNotIn("exitCode",value["events"][-1])

    def test_identity_rejects_wrong_uid_host_and_birth(self):
        monitor=lifecycle.ShimMonitor("/private/unique/Shim",123,Mock())
        monitor.lib=Mock()
        born=monitor.started_wall+1
        observed_uid=os.geteuid()
        def path(pid, buffer, size):
            ctypes.memmove(buffer,b"/private/unique/Shim"+bytes([0]),21)
            return 20
        def info(pid, flavor, arg, buffer, size):
            value=ctypes.cast(buffer,ctypes.POINTER(lifecycle.BSDInfo)).contents
            value.pid=42;value.uid=observed_uid;value.start_sec=int(born);value.start_usec=int((born-int(born))*1000000)
            return ctypes.sizeof(lifecycle.BSDInfo)
        monitor.lib.proc_pidpath.side_effect=path
        monitor.lib.proc_pidinfo.side_effect=info
        command="/private/unique/Shim --host-service test --host-pid 123 --launch-token "+"a"*64
        monitor.process_rows.return_value=[(42,"unused",command)]
        self.assertEqual(ctypes.sizeof(lifecycle.BSDInfo),136)
        self.assertIsNotNone(monitor._identity(42))
        observed_uid=os.geteuid()+1
        self.assertIsNone(monitor._identity(42))
        observed_uid=os.geteuid()
        monitor.process_rows.return_value=[(42,"unused",command.replace("123","456"))]
        self.assertIsNone(monitor._identity(42))
        monitor.process_rows.return_value=[(42,"unused",command)]
        born=monitor.started_wall-1
        self.assertIsNone(monitor._identity(42))

    def test_crash_report_requires_private_executable_pid_and_start(self):
        with tempfile.TemporaryDirectory() as directory:
            now=time.time()-0.1
            report={"pid":42,"procPath":"/private/unique/Shim","procLaunch":
                datetime.datetime.fromtimestamp(now,datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S.%f %z"),
                "exception":{"type":"EXC_CRASH","signal":"SIGABRT"},
                "termination":{"namespace":"DYLD","code":1,"indicator":"PRIVATE"},
                "asi":{"dyld":["Library not loaded: libmozglue.dylib PRIVATE"]},"token":"PRIVATE"}
            target=Path(directory)/"floorp-app-shim-unit.ips"
            target.write_text(json.dumps({"metadata":"PRIVATE"})+"\n"+json.dumps(report))
            (Path(directory)/"floorp-app-shim-z-malformed.ips").write_text("[]")
            value=lifecycle.crash_summary(directory,42,"/private/unique/Shim",now-1,now)
            self.assertEqual(value["matchedReports"][0]["terminationNamespace"],"DYLD")
            self.assertNotIn("PRIVATE",json.dumps(value))
            for pid,exe,start in [(43,"/private/unique/Shim",now),(42,"/other",now),(42,"/private/unique/Shim",now-10)]:
                self.assertEqual(lifecycle.crash_summary(directory,pid,exe,now-1,start)["matchedReports"],[])

if __name__ == "__main__":
    unittest.main()
