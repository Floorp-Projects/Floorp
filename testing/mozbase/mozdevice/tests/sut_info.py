#/usr/bin/env python
import mozdevice
import logging
import re
import unittest
from sut import MockAgent


class TestGetInfo(unittest.TestCase):

    commands = {'os': ('info os', 'JDQ39'),
                'id': ('info id', '11:22:33:44:55:66'),
                'uptime': ('info uptime', '0 days 0 hours 7 minutes 0 seconds 0 ms'),
                'uptimemillis': ('info uptimemillis', '666'),
                'systime': ('info systime', '2013/04/2 12:42:00:007'),
                'screen': ('info screen', 'X:768 Y:1184'),
                'rotation': ('info rotation', 'ROTATION:0'),
                'memory': ('info memory', 'PA:1351032832, FREE: 878645248'),
                'process': ('info process', '1000    527     system\n'
                            '10091   3443    org.mozilla.firefox\n'
                            '10112   3137    com.mozilla.SUTAgentAndroid\n'
                            '10035   807     com.android.launcher'),
                'disk': ('info disk', '/data: 6084923392 total, 980922368 available\n'
                         '/system: 867999744 total, 332333056 available\n'
                         '/mnt/sdcard: 6084923392 total, 980922368 available'),
                'power': ('info power', 'Power status:\n'
                          '  AC power OFFLINE\n'
                          '  Battery charge LOW DISCHARGING\n'
                          '  Remaining charge:      20%\n'
                          '  Battery Temperature:   25.2 (c)'),
                'sutuserinfo': ('info sutuserinfo', 'User Serial:0'),
                'temperature': ('info temperature', 'Temperature: unknown')
                }

    def test_getInfo(self):

        for directive in self.commands.keys():
            m = MockAgent(self, commands=[self.commands[directive]])
            d = mozdevice.DroidSUT('127.0.0.1', port=m.port, logLevel=logging.DEBUG)

            expected = re.sub(r'\ +', ' ', self.commands[directive][1]).split('\n')
            # Account for slightly different return format for 'process'
            if directive is 'process':
                expected = [[x] for x in expected]

            self.assertEqual(d.getInfo(directive=directive)[directive], expected)

if __name__ == '__main__':
    unittest.main()
