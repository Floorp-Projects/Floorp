#!/usr/bin/python
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses |atos| to post-process the entries produced by
# NS_FormatCodeAddress(), which on Mac often lack a file name and a line
# number.

import subprocess
import sys
import re
import os
import pty
import termios

class unbufferedLineConverter:
    """
    Wrap a child process that responds to each line of input with one line of
    output.  Uses pty to trick the child into providing unbuffered output.
    """
    def __init__(self, command, args = []):
        pid, fd = pty.fork()
        if pid == 0:
            # We're the child.  Transfer control to command.
            os.execvp(command, [command] + args)
        else:
            # Disable echoing.
            attr = termios.tcgetattr(fd)
            attr[3] = attr[3] & ~termios.ECHO
            termios.tcsetattr(fd, termios.TCSANOW, attr)
            # Set up a file()-like interface to the child process
            self.r = os.fdopen(fd, "r", 1)
            self.w = os.fdopen(os.dup(fd), "w", 1)
    def convert(self, line):
        self.w.write(line + "\n")
        return self.r.readline().rstrip("\r\n")
    @staticmethod
    def test():
        assert unbufferedLineConverter("rev").convert("123") == "321"
        assert unbufferedLineConverter("cut", ["-c3"]).convert("abcde") == "c"
        print "Pass"

def separate_debug_file_for(file):
    return None

address_adjustments = {}
def address_adjustment(file):
    if not file in address_adjustments:
        result = None
        otool = subprocess.Popen(["otool", "-l", file], stdout=subprocess.PIPE)
        while True:
            line = otool.stdout.readline()
            if line == "":
                break
            if line == "  segname __TEXT\n":
                line = otool.stdout.readline()
                if not line.startswith("   vmaddr "):
                    raise StandardError("unexpected otool output")
                result = int(line[10:], 16)
                break
        otool.stdout.close()

        if result is None:
            raise StandardError("unexpected otool output")

        address_adjustments[file] = result

    return address_adjustments[file]

atoses = {}
def addressToSymbol(file, address):
    converter = None
    if not file in atoses:
        debug_file = separate_debug_file_for(file) or file
        converter = unbufferedLineConverter('/usr/bin/xcrun', ['atos', '-arch', 'x86_64', '-o', debug_file])
        atoses[file] = converter
    else:
        converter = atoses[file]
    return converter.convert("0x%X" % address)

cxxfilt_proc = None
def cxxfilt(sym):
    if cxxfilt_proc is None:
        # --no-strip-underscores because atos already stripped the underscore
        globals()["cxxfilt_proc"] = subprocess.Popen(['c++filt',
                                                      '--no-strip-underscores',
                                                      '--format', 'gnu-v3'],
                                                     stdin=subprocess.PIPE,
                                                     stdout=subprocess.PIPE)
    cxxfilt_proc.stdin.write(sym + "\n")
    return cxxfilt_proc.stdout.readline().rstrip("\n")

# Matches lines produced by NS_FormatCodeAddress().
line_re = re.compile("^(.*#\d+: )(.+)\[(.+) \+(0x[0-9A-Fa-f]+)\](.*)$")
atos_name_re = re.compile("^(.+) \(in ([^)]+)\) \((.+)\)$")

def fixSymbols(line):
    result = line_re.match(line)
    if result is not None:
        (before, fn, file, address, after) = result.groups()
        address = int(address, 16)

        if os.path.exists(file) and os.path.isfile(file):
            address += address_adjustment(file)
            info = addressToSymbol(file, address)

            # atos output seems to have three forms:
            #   address
            #   address (in foo.dylib)
            #   symbol (in foo.dylib) (file:line)
            name_result = atos_name_re.match(info)
            if name_result is not None:
                # Print the first two forms as-is, and transform the third
                (name, library, fileline) = name_result.groups()
                # atos demangles, but occasionally it fails.  cxxfilt can mop
                # up the remaining cases(!), which will begin with '_Z'.
                if (name.startswith("_Z")):
                    name = cxxfilt(name)
                info = "%s (%s, in %s)" % (name, fileline, library)

            nl = '\n' if line[-1] == '\n' else ''
            return before + info + after + nl
        else:
            sys.stderr.write("Warning: File \"" + file + "\" does not exist.\n")
            return line
    else:
        return line

if __name__ == "__main__":
    for line in sys.stdin:
        sys.stdout.write(fixSymbols(line))
