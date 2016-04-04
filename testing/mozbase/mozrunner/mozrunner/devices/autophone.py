# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import json
import logging
import os
import shutil
import signal
import socket
import sys
import threading
import time
import which
import BaseHTTPServer
import SimpleHTTPServer

from mozbuild.virtualenv import VirtualenvManager
from mozdevice import DeviceManagerADB
from mozprocess import ProcessHandler


class AutophoneRunner(object):
    """
       Supporting the mach 'autophone' command: configure, run autophone.
    """
    config = {'base-dir' : None,
              'requirements-installed' : False,
              'devices-configured' : False,
              'test-manifest' : None }
    CONFIG_FILE = os.path.join(os.path.expanduser('~'), '.mozbuild', 'autophone.json')

    def __init__(self, build_obj, verbose):
        self.build_obj = build_obj
        self.verbose = verbose
        self.autophone_options = []
        self.httpd = None
        self.webserver_required = False

    def reset_to_clean(self):
        """
           If confirmed, remove the autophone directory and configuration.
        """
        dir = self.config['base-dir']
        if dir and os.path.exists(dir) and os.path.exists(self.CONFIG_FILE):
            self.build_obj.log(logging.WARN, "autophone", {},
                "*** This will delete %s and reset your 'mach autophone' configuration! ***" % dir)
            response = raw_input(
                "Proceed with deletion? (y/N) ").strip()
            if response.lower().startswith('y'):
                os.remove(self.CONFIG_FILE)
                shutil.rmtree(dir)
        else:
            self.build_obj.log(logging.INFO, "autophone", {},
                "Already clean -- nothing to do!")

    def save_config(self):
        """
           Persist self.config to a file.
        """
        try:
            with open(self.CONFIG_FILE, 'w') as f:
                json.dump(self.config, f)
            if self.verbose:
                print("saved configuration: %s" % self.config)
        except:
            self.build_obj.log(logging.ERROR, "autophone", {},
                "unable to save 'mach autophone' configuration to %s" % self.CONFIG_FILE)
            if self.verbose:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    str(sys.exc_info()[0]))

    def load_config(self):
        """
           Import the configuration info saved by save_config().
        """
        if os.path.exists(self.CONFIG_FILE):
            try:
                with open(self.CONFIG_FILE, 'r') as f:
                    self.config = json.load(f)
                if self.verbose:
                    print("loaded configuration: %s" % self.config)
            except:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    "unable to load 'mach autophone' configuration from %s" % self.CONFIG_FILE)
                if self.verbose:
                    self.build_obj.log(logging.ERROR, "autophone", {},
                        str(sys.exc_info()[0]))

    def setup_directory(self):
        """
           Find the autophone source code location, or download if necessary.
        """
        keep_going = True
        dir = self.config['base-dir']
        if not dir:
            dir = os.path.join(os.path.expanduser('~'), 'mach-autophone')
        if os.path.exists(os.path.join(dir, '.git')):
            response = raw_input(
                "Run autophone from existing directory, %s (Y/n) " % dir).strip()
            if not 'n' in response.lower():
                self.build_obj.log(logging.INFO, "autophone", {},
                    "Configuring and running autophone at %s" % dir)
                return keep_going
        self.build_obj.log(logging.INFO, "autophone", {},
            "Unable to find an existing autophone directory. Let's setup a new one...")
        response = raw_input(
            "Enter location of new autophone directory: [%s] " % dir).strip()
        if response != '':
            dir = response
        self.config['base-dir'] = dir
        if not os.path.exists(os.path.join(dir, '.git')):
            self.build_obj.log(logging.INFO, "autophone", {},
                "Cloning autophone repository to '%s'..." % dir)
            self.config['requirements-installed'] = False
            self.config['devices-configured'] = False
            self.run_process(['git', 'clone', 'https://github.com/mozilla/autophone', dir])
            self.run_process(['git', 'submodule', 'update', '--init', '--remote'], cwd=dir)
        if not os.path.exists(os.path.join(dir, '.git')):
            # git not installed? File permission problem? github not available?
            self.build_obj.log(logging.ERROR, "autophone", {},
                "Unable to clone autophone directory.")
            if not self.verbose:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    "Try re-running this command with --verbose to get more info.")
            keep_going = False
        return keep_going

    def install_requirements(self):
        """
           Install required python modules in a virtualenv rooted at <autophone>/_virtualenv.
        """
        keep_going = True
        dir = self.config['base-dir']
        vdir = os.path.join(dir, '_virtualenv')
        self.auto_virtualenv_manager = VirtualenvManager(self.build_obj.topsrcdir,
            self.build_obj.topobjdir, vdir, sys.stdout,
            os.path.join(self.build_obj.topsrcdir, 'build', 'virtualenv_packages.txt'))
        if not self.config['requirements-installed'] or not os.path.exists(vdir):
            self.build_obj.log(logging.INFO, "autophone", {},
                "Installing required modules in a virtualenv...")
            self.auto_virtualenv_manager.build()
            self.auto_virtualenv_manager._run_pip(['install', '-r',
                os.path.join(dir, 'requirements.txt')])
            self.config['requirements-installed'] = True
        return keep_going

    def configure_devices(self):
        """
           Ensure devices.ini is set up.
        """
        keep_going = True
        device_ini = os.path.join(self.config['base-dir'], 'devices.ini')
        if os.path.exists(device_ini):
            response = raw_input(
                "Use existing device configuration at %s? (Y/n) " % device_ini).strip()
            if not 'n' in response.lower():
                self.build_obj.log(logging.INFO, "autophone", {},
                    "Using device configuration at %s" % device_ini)
                return keep_going
        keep_going = False
        self.build_obj.log(logging.INFO, "autophone", {},
            "You must configure at least one Android device before running autophone.")
        response = raw_input(
            "Configure devices now? (Y/n) ").strip()
        if response.lower().startswith('y') or response == '':
            response = raw_input(
                "Connect your rooted Android test device(s) with usb and press Enter ")
            adb_path = 'adb'
            try:
                if os.path.exists(self.build_obj.substs["ADB"]):
                    adb_path = self.build_obj.substs["ADB"]
            except:
                if self.verbose:
                    self.build_obj.log(logging.ERROR, "autophone", {},
                        str(sys.exc_info()[0]))
                # No build environment?
                try:
                    adb_path = which.which('adb')
                except which.WhichError:
                    adb_path = raw_input(
                        "adb not found. Enter path to adb: ").strip()
            if self.verbose:
                print("Using adb at %s" % adb_path)
            dm = DeviceManagerADB(autoconnect=False, adbPath=adb_path, retryLimit=1)
            device_index = 1
            try:
                with open(os.path.join(self.config['base-dir'], 'devices.ini'), 'w') as f:
                    for device in dm.devices():
                        serial = device[0]
                        if self.verify_device(adb_path, serial):
                            f.write("[device-%d]\nserialno=%s\n" % (device_index, serial))
                            device_index += 1
                            self.build_obj.log(logging.INFO, "autophone", {},
                                "Added '%s' to device configuration." % serial)
                            keep_going = True
                        else:
                            self.build_obj.log(logging.WARNING, "autophone", {},
                                "Device '%s' is not rooted - skipping" % serial)
            except:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    "Failed to get list of connected Android devices.")
                if self.verbose:
                    self.build_obj.log(logging.ERROR, "autophone", {},
                        str(sys.exc_info()[0]))
                keep_going = False
            if device_index <= 1:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    "No devices configured! (Can you see your rooted test device(s) in 'adb devices'?")
                keep_going = False
            if keep_going:
                self.config['devices-configured'] = True
        return keep_going

    def configure_tests(self):
        """
           Determine the required autophone --test-path option.
        """
        dir = self.config['base-dir']
        self.build_obj.log(logging.INFO, "autophone", {},
            "Autophone must be started with a 'test manifest' describing the type(s) of test(s) to run.")
        test_options = []
        for ini in glob.glob(os.path.join(dir, 'tests', '*.ini')):
            with open(ini, 'r') as f:
                content = f.readlines()
                for line in content:
                    if line.startswith('# @mach@ '):
                        webserver = False
                        if '@webserver@' in line:
                            webserver = True
                            line = line.replace('@webserver@', '')
                        test_options.append((line[9:].strip(), ini, webserver))
                        break
        if len(test_options) >= 1:
            test_options.sort()
            self.build_obj.log(logging.INFO, "autophone", {},
                "These test manifests are available:")
            index = 1
            for option in test_options:
                print("%d. %s" % (index, option[0]))
                index += 1
            highest = index - 1
            path = None
            while not path:
                path = None
                self.webserver_required = False
                response = raw_input(
                    "Select test manifest (1-%d, or path to test manifest) " % highest).strip()
                if os.path.isfile(response):
                    path = response
                    self.config['test-manifest'] = path
                    # Assume a webserver is required; if it isn't, user can provide a dummy url.
                    self.webserver_required = True
                else:
                    try:
                        choice = int(response)
                        if choice >= 1 and choice <= highest:
                            path = test_options[choice-1][1]
                            if test_options[choice-1][2]:
                                self.webserver_required = True
                        else:
                            self.build_obj.log(logging.ERROR, "autophone", {},
                                "'%s' invalid: Enter a number between 1 and %d!" % (response, highest))
                    except ValueError:
                        self.build_obj.log(logging.ERROR, "autophone", {},
                            "'%s' unrecognized: Enter a number between 1 and %d!" % (response, highest))
            self.autophone_options.extend(['--test-path', path])
        else:
            # Provide a simple backup for the unusual case where test manifests
            # cannot be found.
            response = ""
            default = self.config['test-manifest'] or ""
            while not os.path.isfile(response):
                response = raw_input(
                    "Enter path to a test manifest: [%s] " % default).strip()
                if response == "":
                    response = default
            self.autophone_options.extend(['--test-path', response])
            self.config['test-manifest'] = response
            # Assume a webserver is required; if it isn't, user can provide a dummy url.
            self.webserver_required = True

        return True

    def write_unittest_defaults(self, defaults_path, xre_path):
        """
           Write unittest-defaults.ini.
        """
        try:
            # This should be similar to unittest-defaults.ini.example
            with open(defaults_path, 'w') as f:
                f.write("""\
# Created by 'mach autophone'
[runtests]
xre_path = %s
utility_path = %s
console_level = DEBUG
log_level = DEBUG
time_out = 300""" % (xre_path, xre_path))
            if self.verbose:
                print("Created %s with host utilities path %s" % (defaults_path, xre_path))
        except:
            self.build_obj.log(logging.ERROR, "autophone", {},
                "Unable to create %s" % defaults_path)
            if self.verbose:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    str(sys.exc_info()[0]))

    def configure_unittests(self):
        """
           Ensure unittest-defaults.ini is set up.
        """
        defaults_path = os.path.join(self.config['base-dir'], 'configs', 'unittest-defaults.ini')
        if os.path.isfile(defaults_path):
            response = raw_input(
                "Use existing unit test configuration at %s? (Y/n) " % defaults_path).strip()
            if 'n' in response.lower():
                os.remove(defaults_path)
        if not os.path.isfile(defaults_path):
            xre_path = os.environ.get('MOZ_HOST_BIN')
            if not xre_path or not os.path.isdir(xre_path):
                emulator_path = os.path.join(os.path.expanduser('~'), '.mozbuild', 'android-device')
                xre_paths = glob.glob(os.path.join(emulator_path, 'host-utils*'))
                for xre_path in xre_paths:
                    if os.path.isdir(xre_path):
                        break
            if not xre_path or not os.path.isdir(xre_path) or \
               not os.path.isfile(os.path.join(xre_path, 'xpcshell')):
                self.build_obj.log(logging.INFO, "autophone", {},
                    "Some tests require access to 'host utilities' such as xpcshell.")
                xre_path = raw_input(
                    "Enter path to host utilities directory: ").strip()
                if not xre_path or not os.path.isdir(xre_path) or \
                   not os.path.isfile(os.path.join(xre_path, 'xpcshell')):
                    self.build_obj.log(logging.ERROR, "autophone", {},
                        "Unable to configure unit tests - no path to host utilities.")
                    return False
            self.write_unittest_defaults(defaults_path, xre_path)
        if os.path.isfile(defaults_path):
            self.build_obj.log(logging.INFO, "autophone", {},
                "Using unit test configuration at %s" % defaults_path)
        return True

    def configure_ip(self):
        """
           Determine what IP should be used for the autophone --ipaddr option.
        """
        # Take a guess at the IP to suggest.  This won't always get the "right" IP,
        # but will save some typing, sometimes.
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 0))
        ip = s.getsockname()[0]
        response = raw_input(
            "IP address of interface to use for phone callbacks [%s] " % ip).strip()
        if response == "":
            response = ip
        self.autophone_options.extend(['--ipaddr', response])
        self.ipaddr = response
        return True

    def configure_webserver(self):
        """
           Determine the autophone --webserver-url option.
        """
        if self.webserver_required:
            self.build_obj.log(logging.INFO, "autophone", {},
                "Some of your selected tests require a webserver.")
            response = raw_input("Start a webserver now? [Y/n] ").strip()
            parts = []
            while len(parts) != 2:
                response2 = raw_input(
                    "Webserver address? [%s:8100] " % self.ipaddr).strip()
                if response2 == "":
                    parts = [self.ipaddr, "8100"]
                else:
                    parts = response2.split(":")
                if len(parts) == 2:
                    addr = parts[0]
                    try:
                        port = int(parts[1])
                        if port <= 0:
                            self.build_obj.log(logging.ERROR, "autophone", {},
                                "Port must be > 0. Enter webserver address in the format <ip>:<port>")
                            parts = []
                    except ValueError:
                        self.build_obj.log(logging.ERROR, "autophone", {},
                            "Port must be a number. Enter webserver address in the format <ip>:<port>")
                        parts = []
                else:
                    self.build_obj.log(logging.ERROR, "autophone", {},
                        "Enter webserver address in the format <ip>:<port>")
            if not ('n' in response.lower()):
                self.launch_webserver(addr, port)
            self.autophone_options.extend(['--webserver-url', 'http://%s:%d' % (addr,port)])
        return True

    def configure_other(self):
        """
           Advanced users may set up additional options in autophone.ini.
           Find and handle that case silently.
        """
        path = os.path.join(self.config['base-dir'], 'autophone.ini')
        if os.path.isfile(path):
            self.autophone_options.extend(['--config', path])
        return True

    def configure(self):
        """
           Ensure all configuration files are set up and determine autophone options.
        """
        return self.configure_devices() and \
               self.configure_unittests() and \
               self.configure_tests() and \
               self.configure_ip() and \
               self.configure_webserver() and \
               self.configure_other()

    def verify_device(self, adb_path, device):
        """
           Check that the specified device is available and rooted.
        """
        try:
            dm = DeviceManagerADB(adbPath=adb_path, retryLimit=1, deviceSerial=device)
            if dm._haveSu or dm._haveRootShell:
                return True
        except:
            self.build_obj.log(logging.WARN, "autophone", {},
                "Unable to verify root on device.")
            if self.verbose:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    str(sys.exc_info()[0]))
        return False

    def launch_autophone(self):
        """
           Launch autophone in its own thread and wait for autophone startup.
        """
        self.build_obj.log(logging.INFO, "autophone", {},
            "Launching autophone...")
        self.thread = threading.Thread(target=self.run_autophone)
        self.thread.start()
        # Wait for startup, so that autophone startup messages do not get mixed
        # in with our interactive command prompts.
        dir = self.config['base-dir']
        started = False
        for seconds in [5, 5, 3, 3, 1, 1, 1, 1]:
            time.sleep(seconds)
            if self.run_process(['./ap.sh', 'autophone-status'], cwd=dir, dump=False):
                started = True
                break
        time.sleep(1)
        if not started:
            self.build_obj.log(logging.WARN, "autophone", {},
                "Autophone is taking longer than expected to start.")

    def run_autophone(self):
        dir = self.config['base-dir']
        cmd = [self.auto_virtualenv_manager.python_path, 'autophone.py']
        cmd.extend(self.autophone_options)
        self.run_process(cmd, cwd=dir, dump=True)

    def command_prompts(self):
        """
           Interactive command prompts: Provide access to ap.sh and trigger_runs.py.
        """
        dir = self.config['base-dir']
        if self.thread.isAlive():
            self.build_obj.log(logging.INFO, "autophone", {},
                "Use 'trigger' to select builds to test using the current test manifest.")
            self.build_obj.log(logging.INFO, "autophone", {},
                "Type 'trigger', 'help', 'quit', or an autophone command.")
        quitting = False
        while self.thread.isAlive() and not quitting:
            response = raw_input(
                "autophone command? ").strip().lower()
            if response == "help":
                self.run_process(['./ap.sh', 'autophone-help'], cwd=dir, dump=True)
                print("""\

Additional commands available in this interactive shell:

trigger
   Initiate autophone test runs. You will be prompted for a set of builds
   to run tests against. (To run a different type of test, quit, run this
   mach command again, and select a new test manifest.)

quit
   Shutdown autophone and exit this shell (short-cut to 'autophone-shutdown')

                      """)
                continue
            if response == "trigger":
                self.trigger_prompts()
                continue
            if response == "quit":
                self.build_obj.log(logging.INFO, "autophone", {},
                    "Quitting...")
                response = "autophone-shutdown"
            if response == "autophone-shutdown":
                quitting = True
            self.run_process(['./ap.sh', response], cwd=dir, dump=True)
        if self.httpd:
            self.httpd.shutdown()
        self.thread.join()

    def trigger_prompts(self):
        """
           Sub-prompts for the "trigger" command.
        """
        dir = self.config['base-dir']
        self.build_obj.log(logging.INFO, "autophone", {},
            "Tests will be run against a build or collection of builds, selected by:")
        print("""\
1. The latest build
2. Build URL
3. Build ID
4. Date/date-time range\
              """)
        highest = 4
        choice = 0
        while (choice < 1 or choice > highest):
            response = raw_input(
                "Build selection type? (1-%d) " % highest).strip()
            try:
                choice = int(response)
            except ValueError:
                self.build_obj.log(logging.ERROR, "autophone", {},
                    "Enter a number between 1 and %d" % highest)
                choice = 0
        if choice == 1:
            options = ["latest"]
        elif choice == 2:
            url = raw_input(
                "Enter url of build to test; may be an http or file schema ").strip()
            options = ["--build-url=%s" % url]
        elif choice == 3:
            response = raw_input(
                "Enter Build ID, eg 20120403063158 ").strip()
            options = [response]
        elif choice == 4:
            start = raw_input(
                "Enter start build date or date-time, e.g. 2012-04-03 or 2012-04-03T06:31:58 ").strip()
            end = raw_input(
                "Enter end build date or date-time, e.g. 2012-04-03 or 2012-04-03T06:31:58 ").strip()
            options = [start, end]
        self.build_obj.log(logging.INFO, "autophone", {},
            "You may optionally specify a repository name like 'mozilla-inbound' or 'try'.")
        self.build_obj.log(logging.INFO, "autophone", {},
            "If not specified, 'mozilla-central' is assumed.")
        repo = raw_input(
            "Enter repository name: ").strip()
        if len(repo) > 0:
            options.extend(["--repo=%s" % repo])
        if repo == "mozilla-central" or repo == "mozilla-aurora" or len(repo) < 1:
            self.build_obj.log(logging.INFO, "autophone", {},
                "You may optionally specify the build location, like 'nightly' or 'tinderbox'.")
            location = raw_input(
                "Enter build location: ").strip()
            if len(location) > 0:
                options.extend(["--build-location=%s" % location])
        else:
            options.extend(["--build-location=tinderbox"])
        cmd = [self.auto_virtualenv_manager.python_path, "trigger_runs.py"]
        cmd.extend(options)
        self.build_obj.log(logging.INFO, "autophone", {},
            "Triggering...Tests will run once builds have been downloaded.")
        self.build_obj.log(logging.INFO, "autophone", {},
            "Use 'autophone-status' to check progress.")
        self.run_process(cmd, cwd=dir, dump=True)

    def launch_webserver(self, addr, port):
        """
           Launch the webserver (in a separate thread).
        """
        self.build_obj.log(logging.INFO, "autophone", {},
            "Launching webserver...")
        self.webserver_addr = addr
        self.webserver_port = port
        self.threadweb = threading.Thread(target=self.run_webserver)
        self.threadweb.start()

    def run_webserver(self):
        class AutoHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
            # A simple request handler with logging suppressed.
            def log_message(self, format, *args):
                pass

        os.chdir(self.config['base-dir'])
        address = (self.webserver_addr, self.webserver_port)
        self.httpd = BaseHTTPServer.HTTPServer(address, AutoHTTPRequestHandler)
        try:
            self.httpd.serve_forever()
        except KeyboardInterrupt:
            print("Web server interrupted.")

    def run_process(self, cmd, cwd=None, dump=False):
        def _processOutput(line):
            if self.verbose or dump:
                print(line)

        if self.verbose:
            self.build_obj.log(logging.INFO, "autophone", {},
                "Running '%s' in '%s'" % (cmd, cwd))
        proc = ProcessHandler(cmd, cwd=cwd, processOutputLine=_processOutput,
            processStderrLine=_processOutput)
        proc.run()
        proc_complete = False
        try:
            proc.wait()
            if proc.proc.returncode == 0:
                proc_complete = True
        except:
            if proc.poll() is None:
                proc.kill(signal.SIGTERM)
        if not proc_complete:
            if not self.verbose:
                print(proc.output)
        return proc_complete
