# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Command-line client to control a device with the SUTAgent software installed
"""

import os
import posixpath
import StringIO
import sys
from optparse import OptionParser

from mozdevice import droid

class SUTCli(object):

    def __init__(self, args=sys.argv[1:]):
        usage = "usage: %prog [options] <command> [<args>]\n\ndevice commands:\n"
        self.commands = { 'install': { 'function': self.install,
                                       'min_args': 1,
                                       'max_args': 1,
                                       'help_args': '<file>',
                                       'help': 'push this package file to the device and install it' },
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
                          'shell': { 'function': self.shell,
                                     'min_args': 1,
                                     'max_args': None,
                                     'help_args': '<command>',
                                     'help': 'run shell command on device' },
                          'info': { 'function': self.getinfo,
                                    'min_args': None,
                                    'max_args': 1,
                                    'help_args': '[os|id|uptime|systime|screen|memory|processes]',
                                    'help': 'get information on a specified '
                                    'aspect of the device (if no argument '
                                    'given, print all available information)'
                                    },
                          'ps': { 'function': self.processlist,
                                    'min_args': None,
                                    'max_args': 0,
                                    'help_args': '',
                                    'help': 'get information on running processes on device'
                                },
                          'ls': { 'function': self.listfiles,
                                  'min_args': 1,
                                  'max_args': 1,
                                  'help_args': '<remote>',
                                  'help': 'list files on device'
                                },
                          'rm': { 'function': lambda file: self.dm.removeFile(file),
                                    'min_args': 1,
                                    'max_args': 1,
                                    'help_args': '<remote>',
                                    'help': 'remove file from device'
                                },
                          'rmdir': { 'function': lambda dir: self.dm.removeDir(dir),
                                    'min_args': 1,
                                    'max_args': 1,
                                    'help_args': '<remote>',
                                    'help': 'recursively remove directory from device'
                                }
                          }

        for (commandname, command) in self.commands.iteritems():
            help_args = command['help_args']
            usage += "  %s - %s\n" % (" ".join([ commandname,
                                                 help_args ]).rstrip(),
                                      command['help'])
        self.parser = OptionParser(usage)
        self.add_options(self.parser)

        (self.options, self.args) = self.parser.parse_args(args)

        if len(self.args) < 1:
            self.parser.error("must specify command")

        if not self.options.deviceip:
            if not os.environ.get('TEST_DEVICE'):
                self.parser.error("Must specify device ip in TEST_DEVICE or "
                                  "with --remoteDevice option")
            else:
                self.options.deviceip = os.environ.get('TEST_DEVICE')

        (command_name, command_args) = (self.args[0], self.args[1:])
        if command_name not in self.commands:
            self.parser.error("Invalid command. Valid commands: %s" %
                              " ".join(self.commands.keys()))

        command = self.commands[command_name]
        if command['min_args'] and len(command_args) < command['min_args'] or \
                command['max_args'] and len(command_args) > command['max_args']:
            self.parser.error("Wrong number of arguments")

        self.dm = droid.DroidSUT(self.options.deviceip,
                                 port=int(self.options.deviceport))
        command['function'](*command_args)

    def add_options(self, parser):
        parser.add_option("-r", "--remoteDevice", action="store",
                          type = "string", dest = "deviceip",
                          help = "Device IP", default=None)
        parser.add_option("-p", "--remotePort", action="store",
                          type = "int", dest = "deviceport",
                          help = "SUTAgent port (defaults to 20701)",
                          default=20701)

    def push(self, src, dest):
        if os.path.isdir(src):
            self.dm.pushDir(src, dest)
        else:
            dest_is_dir = dest[-1] == '/' or self.dm.isDir(dest)
            dest = posixpath.normpath(dest)
            if dest_is_dir:
                dest = posixpath.join(dest, os.path.basename(src))
            self.dm.pushFile(src, dest)

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

    def processlist(self):
        pslist = self.dm.getProcessList()
        for ps in pslist:
            print " ".join(ps)

    def listfiles(self, dir):
        filelist = self.dm.listFiles(dir)
        for file in filelist:
            print file

def cli(args=sys.argv[1:]):
    # process the command line
    cli = SUTCli(args)

if __name__ == '__main__':
    cli()
