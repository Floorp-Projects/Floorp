#/usr/bin/env python
import mozdevice
import mozlog
import unittest
from sut import MockAgent


class TestGetIP(unittest.TestCase):
    """ class to test IP methods """

    commands = [('exec ifconfig eth0', 'eth0: ip 192.168.0.1 '
                 'mask 255.255.255.0 flags [up broadcast running multicast]\n'
                 'return code [0]'),
                ('exec ifconfig wlan0', 'wlan0: ip 10.1.39.126\n'
                 'mask 255.255.0.0 flags [up broadcast running multicast]\n'
                 'return code [0]'),
                ('exec ifconfig fake0', '##AGENT-WARNING## [ifconfig] '
                 'command with arg(s) = [fake0] is currently not implemented.')
                ]

    def test_getIP_eth0(self):
        m = MockAgent(self, commands=[self.commands[0]])
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual('192.168.0.1', d.getIP(interfaces=['eth0']))

    def test_getIP_wlan0(self):
        m = MockAgent(self, commands=[self.commands[1]])
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual('10.1.39.126', d.getIP(interfaces=['wlan0']))

    def test_getIP_error(self):
        m = MockAgent(self, commands=[self.commands[2]])
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertRaises(mozdevice.DMError, d.getIP, interfaces=['fake0'])

if __name__ == '__main__':
    unittest.main()
