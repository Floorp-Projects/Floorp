#!/usr/bin/python
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses atos to process the output of nsTraceRefcnt's Mac OS
# X stack walking code.  This is useful for two things:
#  (1) Getting line number information out of
#      |nsTraceRefcntImpl::WalkTheStack|'s output in debug builds.
#  (2) Getting function names out of |nsTraceRefcntImpl::WalkTheStack|'s
#      output on all builds (where it mostly prints UNKNOWN because only
#      a handful of symbols are exported from component libraries).
#
# Use the script by piping output containing stacks (such as raw stacks
# or make-tree.pl balance trees) through this script.

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
        return (self.r.readline().rstrip("\r\n"), self.r.readline().rstrip("\r\n"))
    @staticmethod
    def test():
        assert unbufferedLineConverter("rev").convert("123") == "321"
        assert unbufferedLineConverter("cut", ["-c3"]).convert("abcde") == "c"
        print "Pass"

def separate_debug_file_for(file):
    return None

addr2lines = {}
def addressToSymbol(file, address):
    converter = None
    if not file in addr2lines:
        debug_file = separate_debug_file_for(file) or file
        converter = unbufferedLineConverter('/usr/bin/addr2line', ['-C', '-f', '-e', debug_file])
        addr2lines[file] = converter
    else:
        converter = addr2lines[file]
    return converter.convert(address)

line_re = re.compile("^(.*) ?\[([^ ]*) \+(0x[0-9a-f]{1,8})\](.*)$")
balance_tree_re = re.compile("^([ \|0-9-]*)")

def fixSymbols(line):
    result = line_re.match(line)
    if result is not None:
        # before allows preservation of balance trees
        # after allows preservation of counts
        (before, file, address, after) = result.groups()
        #address = int(address, 16)

        if os.path.exists(file) and os.path.isfile(file):
            #address += address_adjustment(file)
            (name, fileline) = addressToSymbol(file, address)
            info = "%s (%s)" % (name, fileline)

            # throw away the bad symbol, but keep balance tree structure
            before = balance_tree_re.match(before).groups()[0]

            return before + info + after + "\n"
        else:
            sys.stderr.write("Warning: File \"" + file + "\" does not exist.\n")
            return line
    else:
        return line

if __name__ == "__main__":
    for line in sys.stdin:
        sys.stdout.write(fixSymbols(line))
