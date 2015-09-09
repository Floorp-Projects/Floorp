from talos import talosconfig
from talos.configuration import YAML
import unittest
import json


#globals
ffox_path = 'test/path/to/firefox'
command_args = [ffox_path, '-profile', 'pathtoprofile', '-tp', 'pathtotpmanifest', '-tpchrome', '-tpmozafterpaint', '-tpnoisy', '-rss', '-tpcycles', '1', '-tppagecycles', '1']
browser_config = {'deviceroot': '', 'dirs': {}, 'test_name_extension': '_paint', 'repository': 'http://hg.mozilla.org/releases/mozilla-release', 'buildid': '20131205075310', 'results_log': 'pathtoresults_log', 'symbols_path': None, 'bcontroller_config': 'pathtobcontroller', 'host': '', 'browser_name': 'Firefox', 'sourcestamp': '39faf812aaec', 'remote': False, 'child_process': 'plugin-container', 'branch_name': '', 'browser_version': '26.0', 'extra_args': '', 'develop': True, 'preferences': {'browser.display.overlaynavbuttons': False, 'extensions.getAddons.get.url': 'http://127.0.0.1/extensions-dummy/repositoryGetURL', 'dom.max_chrome_script_run_time': 0, 'network.proxy.type': 1, 'extensions.update.background.url': 'http://127.0.0.1/extensions-dummy/updateBackgroundURL', 'network.proxy.http': 'localhost', 'plugins.update.url': 'http://127.0.0.1/plugins-dummy/updateCheckURL', 'dom.max_script_run_time': 0, 'extensions.update.enabled': False, 'browser.safebrowsing.keyURL': 'http://127.0.0.1/safebrowsing-dummy/newkey', 'media.navigator.permission.disabled': True, 'app.update.enabled': False, 'extensions.blocklist.url': 'http://127.0.0.1/extensions-dummy/blocklistURL', 'browser.EULA.override': True, 'extensions.checkCompatibility': False, 'talos.logfile': 'pathtofile', 'browser.safebrowsing.gethashURL': 'http://127.0.0.1/safebrowsing-dummy/gethash', 'extensions.hotfix.url': 'http://127.0.0.1/extensions-dummy/hotfixURL', 'dom.disable_window_move_resize': True, 'network.proxy.http_port': 80, 'browser.dom.window.dump.enabled': True, 'extensions.update.url': 'http://127.0.0.1/extensions-dummy/updateURL', 'browser.chrome.dynamictoolbar': False, 'toolkit.telemetry.notifiedOptOut': 999, 'browser.link.open_newwindow': 2, 'extensions.getAddons.search.url': 'http://127.0.0.1/extensions-dummy/repositorySearchURL', 'browser.cache.disk.smart_size.first_run': False, 'security.turn_off_all_security_so_that_viruses_can_take_over_this_computer': True, 'dom.disable_open_during_load': False, 'extensions.getAddons.search.browseURL': 'http://127.0.0.1/extensions-dummy/repositoryBrowseURL', 'browser.cache.disk.smart_size.enabled': False, 'extensions.getAddons.getWithPerformance.url': 'http://127.0.0.1/extensions-dummy/repositoryGetWithPerformanceURL', 'hangmonitor.timeout': 0, 'extensions.getAddons.maxResults': 0, 'dom.send_after_paint_to_content': True, 'security.fileuri.strict_origin_policy': False, 'media.capturestream_hints.enabled': True, 'extensions.update.notifyUser': False, 'extensions.blocklist.enabled': False, 'browser.bookmarks.max_backups': 0, 'browser.shell.checkDefaultBrowser': False, 'media.peerconnection.enabled': True, 'dom.disable_window_flip': True, 'security.enable_java': False, 'toolkit.telemetry.prompted': 999, 'browser.warnOnQuit': False, 'media.navigator.enabled': True, 'browser.safebrowsing.updateURL': 'http://127.0.0.1/safebrowsing-dummy/update', 'dom.allow_scripts_to_close_windows': True, 'extensions.webservice.discoverURL': 'http://127.0.0.1/extensions-dummy/discoveryURL'}, 'test_timeout': 1200, 'title': 'qm-pxp01', 'error_filename': 'pathtoerrorfile', 'webserver': 'localhost:15707', 'browser_path':ffox_path, 'port': 20701, 'browser_log': 'browser_output.txt', 'process': 'firefox.exe', 'xperf_path': 'C:/Program Files/Microsoft Windows Performance Toolkit/xperf.exe', 'extensions': ['pathtopageloader'], 'fennecIDs': '', 'env': {'NO_EM_RESTART': '1'}, 'init_url': 'http://localhost:15707/getInfo.html', 'browser_wait': 5}
test_config = {'remote_counters': [], 'filters': [['ignore_first', [5]], ['median', []]], 'xperf_user_providers': ['Mozilla Generic Provider', 'Microsoft-Windows-TCPIP'], 'tpcycles': 1, 'browser_log': 'browser_output.txt', 'shutdown': False, 'fennecIDs': False, 'responsiveness': False, 'tpmozafterpaint': True, 'cleanup': 'pathtofile', 'tprender': False, 'xperf_counters': ['main_startup_fileio', 'main_startup_netio', 'main_normal_fileio', 'main_normal_netio', 'nonmain_startup_fileio', 'nonmain_normal_fileio', 'nonmain_normal_netio', 'mainthread_readcount', 'mainthread_readbytes', 'mainthread_writecount', 'mainthread_writebytes'], 'mac_counters': [], 'tpnoisy': True, 'tppagecycles': 1, 'tploadaboutblank': False, 'xperf_providers': ['PROC_THREAD', 'LOADER', 'HARD_FAULTS', 'FILENAME', 'FILE_IO', 'FILE_IO_INIT'], 'rss': True, 'profile_path': 'path', 'name': 'tp5n', 'url': '-tp pathtotp5n.manifest -tpchrome -tpmozafterpaint -tpnoisy -rss -tpcycles 1 -tppagecycles 1', 'setup': 'pathtosetup', 'linux_counters': [], 'tpmanifest': 'pathtotp5n.manifest', 'w7_counters': [], 'timeout': 1800, 'xperf_stackwalk': ['FileCreate', 'FileRead', 'FileWrite', 'FileFlush', 'FileClose'], 'win_counters': [], 'cycles': 1, 'resolution': 20, 'tpchrome': True}


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

    def validate(self,var1, var2):
        # Function to check whether the output generated is correct or not. If the output generated is not correct then specify the expected output to be generated.
        if var1 == var2:
            return 1
        else:
            print "input '%s' != expected '%s'"%(var1,var2)

    def test_talosconfig(self):
        # This function stimulates a call to generateTalosConfig in talosconfig.py . It is then tested whether the output generated is correct or not.

        browser_config_copy = browser_config.copy()
        test_config_copy = test_config.copy()
        test = talosconfig.generateTalosConfig(command_args, browser_config_copy, test_config_copy)

        # ensure that the output generated in yaml file is as expected or not.
        yaml = YAML()
        content = yaml.read(browser_config['bcontroller_config'])
        self.validate(content['command'],"test/path/to/firefox -profile pathtoprofile -tp pathtotpmanifest -tpchrome -tpmozafterpaint -tpnoisy -rss -tpcycles 1 -tppagecycles 1")
        self.validate(content['child_process'],"plugin-container")
        self.validate(content['process'],"firefox.exe")
        self.validate(content['browser_wait'],5)
        self.validate(content['test_timeout'],1200)
        self.validate(content['browser_log'],"browser_output.txt")
        self.validate(content['browser_path'],"test/path/to/firefox")
        self.validate(content['error_filename'],"pathtoerrorfile")
        self.validate(content['xperf_path'],"C:/Program Files/Microsoft Windows Performance Toolkit/xperf.exe")
        self.validate(content['buildid'],20131205075310L)
        self.validate(content['sourcestamp'],"39faf812aaec")
        self.validate(content['repository'],"http://hg.mozilla.org/releases/mozilla-release")
        self.validate(content['title'],"qm-pxp01")
        self.validate(content['testname'],"tp5n")
        self.validate(content['xperf_providers'],['PROC_THREAD', 'LOADER', 'HARD_FAULTS', 'FILENAME', 'FILE_IO', 'FILE_IO_INIT'])
        self.validate(content['xperf_user_providers'],['Mozilla Generic Provider', 'Microsoft-Windows-TCPIP'])
        self.validate(content['xperf_stackwalk'],['FileCreate', 'FileRead', 'FileWrite', 'FileFlush', 'FileClose'])
        self.validate(content['processID'],"None")
        self.validate(content['approot'],"test/path/to")

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
            self.validate(content['xperf_path'],"C:/Program Files/Microsoft Windows Performance Toolkit/xperf.exe")

        # Test to see if keyerror is raised or not for calling testname when xperf_path is missing
        with self.assertRaises(KeyError):
            self.validate(content['testname'],"tp5n")

        # Testing that error is correctly raised or not if xperf_providers is missing
        browser_config_copy = browser_config.copy()
        test_config_copy = test_config.copy()
        del test_config_copy['xperf_providers']
        talosconfig.generateTalosConfig(command_args, browser_config_copy, test_config_copy)
        yaml = YAML()
        content = yaml.read(browser_config['bcontroller_config'])

        # Test to see if keyerror is raised or not when calling xperf_providers
        with self.assertRaises(KeyError):
            self.validate(content['xperf_providers'],['PROC_THREAD', 'LOADER', 'HARD_FAULTS', 'FILENAME', 'FILE_IO', 'FILE_IO_INIT'])

        # Test to see if keyerror is raised or not when calling xperf_user_providers when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['xperf_user_providers'],['Mozilla Generic Provider', 'Microsoft-Windows-TCPIP'])

        # Test to see if keyerror is raised or not when calling xperf_stackwalk when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['xperf_stackwalk'],['FileCreate', 'FileRead', 'FileWrite', 'FileFlush', 'FileClose'])

        # Test to see if keyerror is raised or not when calling processID when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['processID'],"None")

        # Test to see if keyerror is raised or not when calling approot when xperf_providers is missing
        with self.assertRaises(KeyError):
            self.validate(content['approot'],"test/path/to")

if __name__ == '__main__':
    unittest.main()
