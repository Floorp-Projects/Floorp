# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Command-line client to control a device
"""

import errno
import os
import posixpath
import StringIO
import sys
import mozdevice
import mozlog
import argparse

class DMCli(object):

    def __init__(self):
        self.commands = { 'deviceroot': { 'function': self.deviceroot,
                                          'help': 'get device root directory for storing temporary files' },
                          'install': { 'function': self.install,
                                       'args': [ { 'name': 'file' } ],
                                       'help': 'push this package file to the device and install it' },
                          'uninstall': { 'function': self.uninstall,
                                         'args': [ { 'name': 'packagename' } ],
                                         'help': 'uninstall the named app from the device' },
                          'killapp': { 'function': self.kill,
                                       'args': [ { 'name': 'process_name', 'nargs': '*' } ],
                                       'help': 'kills any processes with name(s) on device' },
                          'launchapp': { 'function': self.launchapp,
                                         'args': [ { 'name': 'appname' },
                                                   { 'name': 'activity_name' },
                                                   { 'name': '--intent',
                                                     'action': 'store',
                                                     'default': 'android.intent.action.VIEW' },
                                                   { 'name': '--url',
                                                     'action': 'store' },
                                                   { 'name': '--no-fail-if-running',
                                                     'action': 'store_true',
                                                     'help': 'Don\'t fail if application is already running' }
                                                ],
                                      'help': 'launches application on device' },
                          'listapps': { 'function': self.listapps,
                                        'help': 'list applications on device' },
                          'push': { 'function': self.push,
                                    'args': [ { 'name': 'local_file' },
                                              { 'name': 'remote_file' }
                                              ],
                                    'help': 'copy file/dir to device' },
                          'pull': { 'function': self.pull,
                                    'args': [ { 'name': 'local_file' },
                                              { 'name': 'remote_file', 'nargs': '?' } ],
                                    'help': 'copy file/dir from device' },
                          'shell': { 'function': self.shell,
                                    'args': [ { 'name': 'command', 'nargs': argparse.REMAINDER },
                                              { 'name': '--root', 'action': 'store_true',
                                                'help': 'Run command as root' }],
                                    'help': 'run shell command on device' },
                          'info': { 'function': self.getinfo,
                                    'args': [ { 'name': 'directive', 'nargs': '?' } ],
                                    'help': 'get information on specified '
                                    'aspect of the device (if no argument '
                                    'given, print all available information)'
                                    },
                          'ps': { 'function': self.processlist,
                                  'help': 'get information on running processes on device'
                                },
                          'logcat' : { 'function': self.logcat,
                                       'help': 'get logcat from device'
                                },
                          'ls': { 'function': self.listfiles,
                                  'args': [ { 'name': 'remote_dir' } ],
                                  'help': 'list files on device'
                                },
                          'rm': { 'function': self.removefile,
                                  'args': [ { 'name': 'remote_file' } ],
                                  'help': 'remove file from device'
                                },
                          'isdir': { 'function': self.isdir,
                                     'args': [ { 'name': 'remote_dir' } ],
                                     'help': 'print if remote file is a directory'
                                },
                          'mkdir': { 'function': self.mkdir,
                                     'args': [ { 'name': 'remote_dir' } ],
                                     'help': 'makes a directory on device'
                                },
                          'rmdir': { 'function': self.rmdir,
                                     'args': [ { 'name': 'remote_dir' } ],
                                     'help': 'recursively remove directory from device'
                                },
                          'screencap': { 'function': self.screencap,
                                         'args': [ { 'name': 'png_file' } ],
                                         'help': 'capture screenshot of device in action'
                                         },
                          'sutver': { 'function': self.sutver,
                                      'help': 'SUTAgent\'s product name and version (SUT only)'
                                   },
                          'clearlogcat': { 'function': self.clearlogcat,
                                           'help': 'clear the logcat'
                                         },
                          'reboot': { 'function': self.reboot,
                                      'help': 'reboot the device',
                                      'args': [ { 'name': '--wait',
                                                  'action': 'store_true',
                                                  'help': 'Wait for device to come back up before exiting' } ]

                                   },
                          'isfile': { 'function': self.isfile,
                                      'args': [ { 'name': 'remote_file' } ],
                                      'help': 'check whether a file exists on the device'
                                   },
                          'launchfennec': { 'function': self.launchfennec,
                                            'args': [ { 'name': 'appname' },
                                                      { 'name': '--intent', 'action': 'store',
                                                        'default': 'android.intent.action.VIEW' },
                                                      { 'name': '--url', 'action': 'store' },
                                                      { 'name': '--extra-args', 'action': 'store' },
                                                      { 'name': '--mozenv', 'action': 'store',
                                                        'help': 'Gecko environment variables to set in "KEY1=VAL1 KEY2=VAL2" format' },
                                                      { 'name': '--no-fail-if-running',
                                                        'action': 'store_true',
                                                        'help': 'Don\'t fail if application is already running' }
                                                      ],
                                            'help': 'launch fennec'
                                            },
                          'getip': { 'function': self.getip,
                                     'args': [ { 'name': 'interface', 'nargs': '*' } ],
                                     'help': 'get the ip address of the device'
                                   }
                          }

        self.parser = argparse.ArgumentParser()
        self.add_options(self.parser)
        self.add_commands(self.parser)
        mozlog.structured.commandline.add_logging_group(self.parser)

    def run(self, args=sys.argv[1:]):
        args = self.parser.parse_args()

        mozlog.structured.commandline.setup_logging(
            'mozdevice', args, {'mach': sys.stdout})

        if args.dmtype == "sut" and not args.host and not args.hwid:
            self.parser.error("Must specify device ip in TEST_DEVICE or "
                              "with --host option with SUT")

        self.dm = self.getDevice(dmtype=args.dmtype, hwid=args.hwid,
                                 host=args.host, port=args.port,
                                 verbose=args.verbose)

        ret = args.func(args)
        if ret is None:
            ret = 0

        sys.exit(ret)

    def add_options(self, parser):
        parser.add_argument("-v", "--verbose", action="store_true",
                            help="Verbose output from DeviceManager",
                            default=bool(os.environ.get('VERBOSE')))
        parser.add_argument("--host", action="store",
                            help="Device hostname (only if using TCP/IP, " \
                                "defaults to TEST_DEVICE environment " \
                                "variable if present)",
                            default=os.environ.get('TEST_DEVICE'))
        parser.add_argument("-p", "--port", action="store",
                            type=int,
                            help="Custom device port (if using SUTAgent or "
                            "adb-over-tcp)", default=None)
        parser.add_argument("-m", "--dmtype", action="store",
                            help="DeviceManager type (adb or sut, defaults " \
                                "to DM_TRANS environment variable, if " \
                                "present, or adb)",
                            default=os.environ.get('DM_TRANS', 'adb'))
        parser.add_argument("-d", "--hwid", action="store",
                            help="HWID", default=None)
        parser.add_argument("--package-name", action="store",
                            help="Packagename (if using DeviceManagerADB)",
                            default=None)

    def add_commands(self, parser):
        subparsers = parser.add_subparsers(title="Commands", metavar="<command>")
        for (commandname, commandprops) in sorted(self.commands.iteritems()):
            subparser = subparsers.add_parser(commandname, help=commandprops['help'])
            if commandprops.get('args'):
                for arg in commandprops['args']:
                    # this is more elegant but doesn't work in python 2.6
                    # (which we still use on tbpl @ mozilla where we install
                    # this package)
                    # kwargs = { k: v for k,v in arg.items() if k is not 'name' }
                    kwargs = {}
                    for (k, v) in arg.items():
                        if k is not 'name':
                            kwargs[k] = v
                    subparser.add_argument(arg['name'], **kwargs)
            subparser.set_defaults(func=commandprops['function'])

    def getDevice(self, dmtype="adb", hwid=None, host=None, port=None,
                  packagename=None, verbose=False):
        '''
        Returns a device with the specified parameters
        '''
        logLevel = mozlog.ERROR
        if verbose:
            logLevel = mozlog.DEBUG

        if hwid:
            return mozdevice.DroidConnectByHWID(hwid, logLevel=logLevel)

        if dmtype == "adb":
            if host and not port:
                port = 5555
            return mozdevice.DroidADB(packageName=packagename,
                                      host=host, port=port,
                                      logLevel=logLevel)
        elif dmtype == "sut":
            if not host:
                self.parser.error("Must specify host with SUT!")
            if not port:
                port = 20701
            return mozdevice.DroidSUT(host=host, port=port,
                                      logLevel=logLevel)
        else:
            self.parser.error("Unknown device manager type: %s" % type)

    def deviceroot(self, args):
        print self.dm.deviceRoot

    def push(self, args):
        (src, dest) = (args.local_file, args.remote_file)
        if os.path.isdir(src):
            self.dm.pushDir(src, dest)
        else:
            dest_is_dir = dest[-1] == '/' or self.dm.dirExists(dest)
            dest = posixpath.normpath(dest)
            if dest_is_dir:
                dest = posixpath.join(dest, os.path.basename(src))
            self.dm.pushFile(src, dest)

    def pull(self, args):
        (src, dest) = (args.local_file, args.remote_file)
        if not self.dm.fileExists(src):
            print 'No such file or directory'
            return
        if not dest:
            dest = posixpath.basename(src)
        if self.dm.dirExists(src):
            self.dm.getDirectory(src, dest)
        else:
            self.dm.getFile(src, dest)

    def install(self, args):
        basename = os.path.basename(args.file)
        app_path_on_device = posixpath.join(self.dm.deviceRoot,
                                            basename)
        self.dm.pushFile(args.file, app_path_on_device)
        self.dm.installApp(app_path_on_device)

    def uninstall(self, args):
        self.dm.uninstallApp(args.packagename)

    def launchapp(self, args):
        self.dm.launchApplication(args.appname, args.activity_name,
                                  args.intent, url=args.url,
                                  failIfRunning=(not args.no_fail_if_running))

    def listapps(self, args):
        for app in self.dm.getInstalledApps():
            print app

    def stopapp(self, args):
        self.dm.stopApplication(args.appname)

    def kill(self, args):
        for name in args.process_name:
            self.dm.killProcess(name)

    def shell(self, args):
        buf = StringIO.StringIO()
        self.dm.shell(args.command, buf, root=args.root)
        print str(buf.getvalue()[0:-1]).rstrip()

    def getinfo(self, args):
        info = self.dm.getInfo(directive=args.directive)
        for (infokey, infoitem) in sorted(info.iteritems()):
            if infokey == "process":
                pass  # skip process list: get that through ps
            elif args.directive is None:
                print "%s: %s" % (infokey.upper(), infoitem)
            else:
                print infoitem

    def logcat(self, args):
        print ''.join(self.dm.getLogcat())

    def clearlogcat(self, args):
        self.dm.recordLogcat()

    def reboot(self, args):
        self.dm.reboot(wait=args.wait)

    def processlist(self, args):
        pslist = self.dm.getProcessList()
        for ps in pslist:
            print " ".join(str(i) for i in ps)

    def listfiles(self, args):
        filelist = self.dm.listFiles(args.remote_dir)
        for file in filelist:
            print file

    def removefile(self, args):
        self.dm.removeFile(args.remote_file)

    def isdir(self, args):
        if self.dm.dirExists(args.remote_dir):
            print "TRUE"
            return

        print "FALSE"
        return errno.ENOTDIR

    def mkdir(self, args):
        self.dm.mkDir(args.remote_dir)

    def rmdir(self, args):
        self.dm.removeDir(args.remote_dir)

    def screencap(self, args):
        self.dm.saveScreenshot(args.png_file)

    def sutver(self, args):
        if args.dmtype == 'sut':
            print '%s Version %s' % (self.dm.agentProductName,
                                     self.dm.agentVersion)
        else:
            print 'Must use SUT transport to get SUT version.'

    def isfile(self, args):
        if self.dm.fileExists(args.remote_file):
            print "TRUE"
            return
        print "FALSE"
        return errno.ENOENT

    def launchfennec(self, args):
        mozEnv = None
        if args.mozenv:
            mozEnv = {}
            keyvals = args.mozenv.split()
            for keyval in keyvals:
                (key, _, val) = keyval.partition("=")
                mozEnv[key] = val
        self.dm.launchFennec(args.appname, intent=args.intent,
                             mozEnv=mozEnv,
                             extraArgs=args.extra_args, url=args.url,
                             failIfRunning=(not args.no_fail_if_running))

    def getip(self, args):
        if args.interface:
            print(self.dm.getIP(args.interface))
        else:
            print(self.dm.getIP())

def cli(args=sys.argv[1:]):
    # process the command line
    cli = DMCli()
    cli.run(args)

if __name__ == '__main__':
    cli()
