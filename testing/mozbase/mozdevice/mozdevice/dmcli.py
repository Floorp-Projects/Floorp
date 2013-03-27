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
import textwrap
import mozdevice
from optparse import OptionParser

class DMCli(object):

    def __init__(self):
        # a value of None for 'max_args' means there is no limit to the number
        # of arguments.  'min_args' should always have an integer value >= 0.
        self.commands = { 'install': { 'function': self.install,
                                       'min_args': 1,
                                       'max_args': 1,
                                       'help_args': '<file>',
                                       'help': 'push this package file to the device and install it' },
                          'uninstall': { 'function': lambda a: self.dm.uninstallApp(a),
                                         'min_args': 1,
                                         'max_args': 1,
                                         'help_args': '<packagename>',
                                         'help': 'uninstall the named app from the device' },
                          'killapp': { 'function': self.killapp,
                                       'min_args': 1,
                                       'max_args': 1,
                                       'help_args': '<process name>',
                                       'help': 'kills any processes with a particular name on device' },
                          'launchapp': { 'function': self.launchapp,
                                         'min_args': 4,
                                         'max_args': 4,
                                         'help_args': '<appname> <activity name> <intent> <URL>',
                                         'help': 'launches application on device' },
                          'push': { 'function': self.push,
                                    'min_args': 2,
                                    'max_args': 2,
                                    'help_args': '<local> <remote>',
                                    'help': 'copy file/dir to device' },
                          'pull': { 'function': self.pull,
                                    'min_args': 1,
                                    'max_args': 2,
                                    'help_args': '<local> [remote]',
                                    'help': 'copy file/dir from device' },
                          'shell': { 'function': self.shell,
                                     'min_args': 1,
                                     'max_args': None,
                                     'help_args': '<command>',
                                     'help': 'run shell command on device' },
                          'info': { 'function': self.getinfo,
                                    'min_args': 0,
                                    'max_args': 1,
                                    'help_args': '[os|id|uptime|systime|screen|memory|processes]',
                                    'help': 'get information on a specified '
                                    'aspect of the device (if no argument '
                                    'given, print all available information)'
                                    },
                          'ps': { 'function': self.processlist,
                                    'min_args': 0,
                                    'max_args': 0,
                                    'help_args': '',
                                    'help': 'get information on running processes on device'
                                },
                          'logcat' : { 'function': self.logcat,
                                       'min_args': 0,
                                       'max_args': 0,
                                       'help_args': '',
                                       'help': 'get logcat from device'
                                },
                          'ls': { 'function': self.listfiles,
                                  'min_args': 1,
                                  'max_args': 1,
                                  'help_args': '<remote>',
                                  'help': 'list files on device'
                                },
                          'rm': { 'function': lambda f: self.dm.removeFile(f),
                                    'min_args': 1,
                                    'max_args': 1,
                                    'help_args': '<remote>',
                                    'help': 'remove file from device'
                                },
                          'isdir': { 'function': self.isdir,
                                     'min_args': 1,
                                     'max_args': 1,
                                     'help_args': '<remote>',
                                     'help': 'print if remote file is a directory'
                                },
                          'mkdir': { 'function': lambda d: self.dm.mkDir(d),
                                     'min_args': 1,
                                     'max_args': 1,
                                     'help_args': '<remote>',
                                     'help': 'makes a directory on device'
                                },
                          'rmdir': { 'function': lambda d: self.dm.removeDir(d),
                                    'min_args': 1,
                                    'max_args': 1,
                                    'help_args': '<remote>',
                                    'help': 'recursively remove directory from device'
                                },
                          'screencap': { 'function': lambda f: self.dm.saveScreenshot(f),
                                          'min_args': 1,
                                          'max_args': 1,
                                          'help_args': '<png file>',
                                          'help': 'capture screenshot of device in action'
                                          },
                          'sutver': { 'function': self.sutver,
                                      'min_args': 0,
                                      'max_args': 0,
                                      'help_args': '',
                                      'help': 'SUTAgent\'s product name and version (SUT only)'
                                   },

                          }

        usage = "usage: %prog [options] <command> [<args>]\n\ndevice commands:\n"
        usage += "\n".join([textwrap.fill("%s %s - %s" %
                                          (cmdname, cmd['help_args'],
                                           cmd['help']),
                                          initial_indent="  ",
                                          subsequent_indent="      ")
                            for (cmdname, cmd) in 
                            sorted(self.commands.iteritems())])

        self.parser = OptionParser(usage)
        self.add_options(self.parser)


    def run(self, args=sys.argv[1:]):
        (self.options, self.args) = self.parser.parse_args(args)

        if len(self.args) < 1:
            self.parser.error("must specify command")

        if self.options.dmtype == "sut" and not self.options.host and \
                not self.options.hwid:
            self.parser.error("Must specify device ip in TEST_DEVICE or "
                              "with --host option with SUT")

        (command_name, command_args) = (self.args[0], self.args[1:])
        if command_name not in self.commands:
            self.parser.error("Invalid command. Valid commands: %s" %
                              " ".join(self.commands.keys()))

        command = self.commands[command_name]
        if (len(command_args) < command['min_args'] or
            (command['max_args'] is not None and len(command_args) > 
             command['max_args'])):
            self.parser.error("Wrong number of arguments")

        self.dm = self.getDevice(dmtype=self.options.dmtype,
                                 hwid=self.options.hwid,
                                 host=self.options.host,
                                 port=self.options.port)
        ret = command['function'](*command_args)
        if ret is None:
            ret = 0

        sys.exit(ret)

    def add_options(self, parser):
        parser.add_option("-v", "--verbose", action="store_true",
                          dest="verbose",
                          help="Verbose output from DeviceManager",
                          default=False)
        parser.add_option("--host", action="store",
                          type="string", dest="host",
                          help="Device hostname (only if using TCP/IP)",
                          default=os.environ.get('TEST_DEVICE'))
        parser.add_option("-p", "--port", action="store",
                          type="int", dest="port",
                          help="Custom device port (if using SUTAgent or "
                          "adb-over-tcp)", default=None)
        parser.add_option("-m", "--dmtype", action="store",
                          type="string", dest="dmtype",
                          help="DeviceManager type (adb or sut, defaults " \
                              "to adb)", default=os.environ.get('DM_TRANS',
                                                                'adb'))
        parser.add_option("-d", "--hwid", action="store",
                          type="string", dest="hwid",
                          help="HWID", default=None)
        parser.add_option("--package-name", action="store",
                          type="string", dest="packagename",
                          help="Packagename (if using DeviceManagerADB)",
                          default=None)

    def getDevice(self, dmtype="adb", hwid=None, host=None, port=None):
        '''
        Returns a device with the specified parameters
        '''
        if self.options.verbose:
            mozdevice.DroidSUT.debug = 4

        if hwid:
            return mozdevice.DroidConnectByHWID(hwid)

        if dmtype == "adb":
            if host and not port:
                port = 5555
            return mozdevice.DroidADB(packageName=self.options.packagename,
                                      host=host, port=port)
        elif dmtype == "sut":
            if not host:
                self.parser.error("Must specify host with SUT!")
            if not port:
                port = 20701
            return mozdevice.DroidSUT(host=host, port=port)
        else:
            self.parser.error("Unknown device manager type: %s" % type)

    def push(self, src, dest):
        if os.path.isdir(src):
            self.dm.pushDir(src, dest)
        else:
            dest_is_dir = dest[-1] == '/' or self.dm.dirExists(dest)
            dest = posixpath.normpath(dest)
            if dest_is_dir:
                dest = posixpath.join(dest, os.path.basename(src))
            self.dm.pushFile(src, dest)

    def pull(self, src, dest=None):
        if not self.dm.fileExists(src):
            print 'No such file or directory'
            return
        if not dest:
            dest = posixpath.basename(src)
        if self.dm.dirExists(src):
            self.dm.getDirectory(src, dest)
        else:
            self.dm.getFile(src, dest)

    def install(self, apkfile):
        basename = os.path.basename(apkfile)
        app_path_on_device = posixpath.join(self.dm.getDeviceRoot(),
                                            basename)
        self.dm.pushFile(apkfile, app_path_on_device)
        self.dm.installApp(app_path_on_device)

    def launchapp(self, appname, activity, intent, url):
        self.dm.launchApplication(appname, activity, intent, url)

    def killapp(self, *args):
        for appname in args:
            self.dm.killProcess(appname)

    def shell(self, *args):
        buf = StringIO.StringIO()
        self.dm.shell(args, buf)
        print str(buf.getvalue()[0:-1]).rstrip()

    def getinfo(self, *args):
        directive=None
        if args:
            directive=args[0]
        info = self.dm.getInfo(directive=directive)
        for (infokey, infoitem) in sorted(info.iteritems()):
            if infokey == "process":
                pass # skip process list: get that through ps
            elif not directive and not infoitem:
                print "%s:" % infokey.upper()
            elif not directive:
                for line in infoitem:
                    print "%s: %s" % (infokey.upper(), line)
            else:
                print "%s" % "\n".join(infoitem)

    def logcat(self):
        print ''.join(self.dm.getLogcat())

    def processlist(self):
        pslist = self.dm.getProcessList()
        for ps in pslist:
            print " ".join(str(i) for i in ps)

    def listfiles(self, dir):
        filelist = self.dm.listFiles(dir)
        for file in filelist:
            print file

    def isdir(self, file):
        if self.dm.dirExists(file):
            print "TRUE"
            return 0

        print "FALSE"
        return errno.ENOTDIR

    def sutver(self):
        if self.options.dmtype == 'sut':
            print '%s Version %s' % (self.dm.agentProductName,
                                     self.dm.agentVersion)
        else:
            print 'Must use SUT transport to get SUT version.'

def cli(args=sys.argv[1:]):
    # process the command line
    cli = DMCli()
    cli.run(args)

if __name__ == '__main__':
    cli()
