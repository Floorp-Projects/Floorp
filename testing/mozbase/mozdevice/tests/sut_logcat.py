#!/usr/bin/env python

import mozdevice
import mozlog
import unittest
from sut import MockAgent


class TestLogCat(unittest.TestCase):
    """ Class to test methods associated with logcat """

    def test_getLogcat(self):

        logcat_output = ("07-17 00:51:10.377 I/SUTAgentAndroid( 2933): onCreate\r\n"
        "07-17 00:51:10.457 D/dalvikvm( 2933): GC_CONCURRENT freed 351K, 17% free 2523K/3008K, paused 5ms+2ms, total 38ms\r\n"
        "07-17 00:51:10.497 I/SUTAgentAndroid( 2933): Caught exception creating file in /data/local/tmp: open failed: EACCES (Permission denied)\r\n"
        "07-17 00:51:10.507 E/SUTAgentAndroid( 2933): ERROR: Cannot access world writeable test root\r\n"
        "07-17 00:51:10.547 D/GeckoHealthRec( 3253): Initializing profile cache.\r\n"
        "07-17 00:51:10.607 D/GeckoHealthRec( 3253): Looking for /data/data/org.mozilla.fennec/files/mozilla/c09kfhne.default/times.json\r\n"
        "07-17 00:51:10.637 D/GeckoHealthRec( 3253): Using times.json for profile creation time.\r\n"
        "07-17 00:51:10.707 D/GeckoHealthRec( 3253): Incorporating environment: times.json profile creation = 1374026758604\r\n"
        "07-17 00:51:10.507 D/GeckoHealthRec( 3253): Requested prefs.\r\n"
        "07-17 06:50:54.907 I/SUTAgentAndroid( 3876): \r\n"
        "07-17 06:50:54.907 I/SUTAgentAndroid( 3876): Total Private Dirty Memory         3176 kb\r\n"
        "07-17 06:50:54.907 I/SUTAgentAndroid( 3876): Total Proportional Set Size Memory 5679 kb\r\n"
        "07-17 06:50:54.907 I/SUTAgentAndroid( 3876): Total Shared Dirty Memory          9216 kb\r\n"
        "07-17 06:55:21.627 I/SUTAgentAndroid( 3876): 127.0.0.1 : execsu /system/bin/logcat -v time -d dalvikvm:I "
        "ConnectivityService:S WifiMonitor:S WifiStateTracker:S wpa_supplicant:S NetworkStateTracker:S\r\n"
        "07-17 06:55:21.827 I/dalvikvm-heap( 3876): Grow heap (frag case) to 3.019MB for 102496-byte allocation\r\n"
        "return code [0]")

        inp = ("execsu /system/bin/logcat -v time -d "
        "dalvikvm:I ConnectivityService:S WifiMonitor:S "
        "WifiStateTracker:S wpa_supplicant:S NetworkStateTracker:S")

        commands = [(inp, logcat_output)]
        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual(logcat_output[:-17].replace('\r\n', '\n').splitlines(True), d.getLogcat())

    def test_recordLogcat(self):

        commands = [("execsu /system/bin/logcat -c", "return code [0]")]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        # No error raised means success
        self.assertEqual(None, d.recordLogcat())

if __name__ == '__main__':
    unittest.main()
