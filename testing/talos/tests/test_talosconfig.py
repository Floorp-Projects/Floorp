from talos import talosconfig
from talos.configuration import YAML
import unittest
import json


# globals
ffox_path = 'test/path/to/firefox'
command_args = [ffox_path,
                '-profile',
                'pathtoprofile',
                '-tp',
                'pathtotpmanifest',
                '-tpchrome',
                '-tpmozafterpaint',
                '-tpnoisy',
                '-rss',
                '-tpcycles',
                '1',
                '-tppagecycles',
                '1']
with open("test_talosconfig_browser_config.json") as json_browser_config:
    browser_config = json.load(json_browser_config)
with open("test_talosconfig_test_config.json") as json_test_config:
    test_config = json.load(json_test_config)


class TestWriteConfig(unittest.TestCase):
    def test_writeConfigFile(self):
        obj = dict(some=123, thing='456', other=789)

        self.assertEquals(
            json.loads(talosconfig.writeConfigFile(obj, ('some', 'thing'))),
            dict(some=123, thing='456')
        )

        # test without keys
        self.assertEquals(
            json.loads(talosconfig.writeConfigFile(obj, None)),
            obj
        )


class TalosConfigUnitTest(unittest.TestCase):
    """
    A class inheriting from unittest.TestCase to test the generateTalosConfig function.
    """

    def validate(self, var1, var2):
        # Function to check whether the output generated is correct or not.
        # If the output generated is not correct then specify the expected output to be generated.
        if var1 == var2:
            return 1
        else:
            print("input '%s' != expected '%s'" % (var1, var2))

    def test_talosconfig(self):
        # This function stimulates a call to generateTalosConfig in talosconfig.py .
        # It is then tested whether the output generated is correct or not.
        # ensure that the output generated in yaml file is as expected or not.
        yaml = YAML()
        content = yaml.read(browser_config['bcontroller_config'])
        self.validate(content['command'],
                      "test/path/to/firefox " +
                      "-profile " +
                      "pathtoprofile " +
                      "-tp " +
                      "pathtotpmanifest " +
                      "-tpchrome " +
                      "-tpmozafterpaint " +
                      "-tpnoisy " +
                      "-rss " +
                      "-tpcycles " +
                      "1 " +
                      "-tppagecycles " +
                      "1")
        self.validate(content['child_process'], "plugin-container")
        self.validate(content['process'], "firefox.exe")
        self.validate(content['browser_wait'], 5)
        self.validate(content['test_timeout'], 1200)
        self.validate(content['browser_log'], "browser_output.txt")
        self.validate(content['browser_path'], "test/path/to/firefox")
        self.validate(content['error_filename'], "pathtoerrorfile")
        self.validate(content['xperf_path'],
                      "C:/Program Files/Microsoft Windows Performance Toolkit/xperf.exe")
        self.validate(content['buildid'], 20131205075310)
        self.validate(content['sourcestamp'], "39faf812aaec")
        self.validate(content['repository'], "http://hg.mozilla.org/releases/mozilla-release")
        self.validate(content['title'], "qm-pxp01")
        self.validate(content['testname'], "tp5n")
        self.validate(content['xperf_providers'], ['PROC_THREAD',
                                                   'LOADER',
                                                   'HARD_FAULTS',
                                                   'FILENAME',
                                                   'FILE_IO',
                                                   'FILE_IO_INIT'])
        self.validate(content['xperf_user_providers'],
                      ['Mozilla Generic Provider', 'Microsoft-Windows-TCPIP'])
        self.validate(content['xperf_stackwalk'],
                      ['FileCreate', 'FileRead', 'FileWrite', 'FileFlush', 'FileClose'])
        self.validate(content['processID'], "None")
        self.validate(content['approot'], "test/path/to")

    def test_errors(self):
        # Tests if errors are correctly raised.

        # Testing that error is correctly raised or not if xperf_path is missing
        browser_config_copy = browser_config.copy()
        test_config_copy = test_config.copy()
        del browser_config_copy['xperf_path']
        talosconfig.generateTalosConfig(command_args, browser_config_copy, test_config_copy)
        yaml = YAML()
        content = yaml.read(browser_config['bcontroller_config'])

        with self.assertRaises(KeyError):
            self.validate(content['xperf_path'],
                          "C:/Program Files/Microsoft Windows Performance Toolkit/xperf.exe")

        # Test to see if keyerror is raised or not for calling testname when xperf_path is missing
        with self.assertRaises(KeyError):
            self.validate(content['testname'], "tp5n")

        # Testing that error is correctly raised or not if xperf_providers is missing
        browser_config_copy = browser_config.copy()
        test_config_copy = test_config.copy()
        del test_config_copy['xperf_providers']
        talosconfig.generateTalosConfig(command_args, browser_config_copy, test_config_copy)
        yaml = YAML()
        content = yaml.read(browser_config['bcontroller_config'])

        # Checking keyerror when calling xperf_providers
        with self.assertRaises(KeyError):
            self.validate(content['xperf_providers'], ['PROC_THREAD', 'LOADER', 'HARD_FAULTS',
                                                       'FILENAME', 'FILE_IO', 'FILE_IO_INIT'])

        # Checking keyerror when calling xperf_user_providers when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['xperf_user_providers'],
                          ['Mozilla Generic Provider', 'Microsoft-Windows-TCPIP'])

        # Checking keyerror when calling xperf_stackwalk when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['xperf_stackwalk'],
                          ['FileCreate', 'FileRead', 'FileWrite', 'FileFlush', 'FileClose'])

        # Checking keyerror when calling processID when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['processID'], "None")

        # Checking keyerror when calling approot when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['approot'], "test/path/to")

if __name__ == '__main__':
    unittest.main()
