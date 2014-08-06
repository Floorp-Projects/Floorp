#!/usr/bin/python
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses addr2line (part of binutils) to process the output of
# nsTraceRefcnt's Linux stack walking code.  This is useful for two
# things:
#  (1) Getting line number information out of
#      |nsTraceRefcnt::WalkTheStack|'s output in debug builds.
#  (2) Getting function names out of |nsTraceRefcnt::WalkTheStack|'s
#      output on optimized builds (where it mostly prints UNKNOWN
#      because only a handful of symbols are exported from component
#      libraries).
#
# Use the script by piping output containing stacks (such as raw stacks
# or make-tree.pl balance trees) through this script.

import subprocess
import sys
import re
import os
import pty
import termios
from StringIO import StringIO

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

objdump_section_re = re.compile("^ [0-9a-f]* ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}).*")
def elf_section(file, section):
    """
    Return the requested ELF section of the file as a str, represented
    as a sequence of bytes.
    """
    # We can read the .gnu_debuglink section using either of:
    #   objdump -s --section=.gnu_debuglink $file
    #   readelf -x .gnu_debuglink $file
    # Since readelf prints things backwards on little-endian platforms
    # for some versions only (backwards on Fedora Core 6, forwards on
    # Fedora 7), use objdump.
    objdump = subprocess.Popen(['objdump', '-s', '--section=' + section, file],
                               stdout=subprocess.PIPE,
                               # redirect stderr so errors don't get printed
                               stderr=subprocess.PIPE)
    (objdump_stdout, objdump_stderr) = objdump.communicate()
    if objdump.returncode != 0:
        return None
    result = ""
    # Turn hexadecimal dump into the bytes it represents
    for line in StringIO(objdump_stdout).readlines():
        m = objdump_section_re.match(line)
        if m:
            for gnum in [0, 1, 2, 3]:
                word = m.groups()[gnum]
                if word != "        ":
                    for idx in [0, 2, 4, 6]:
                        result += chr(int(word[idx:idx+2], 16))
    return result

# FIXME: Hard-coded to gdb defaults (works on Fedora and Ubuntu).
global_debug_dir = '/usr/lib/debug';

endian_re = re.compile("\s*Data:\s+.*(little|big) endian.*$")

def separate_debug_file_for(file):
    """
    Finds a separated file with the debug sections for a binary.  Such
    files are commonly installed by debug packages on linux distros.
    Rules for finding them are documented in:
    https://sourceware.org/gdb/current/onlinedocs/gdb/Separate-Debug-Files.html
    """
    def have_debug_file(debugfile):
        return os.path.isfile(debugfile)

    endian = None
    readelf = subprocess.Popen(['readelf', '-h', file],
                               stdout=subprocess.PIPE)
    for line in readelf.stdout.readlines():
        m = endian_re.match(line)
        if m:
            endian = m.groups()[0]
            break
    readelf.terminate()
    if endian is None:
        sys.stderr.write("Could not determine endianness of " + file + "\n")
        return None

    def word32(s):
        if type(s) != str or len(s) != 4:
            raise StandardError("expected 4 byte string input")
        s = list(s)
        if endian == "big":
            s.reverse()
        return sum(map(lambda idx: ord(s[idx]) * (256 ** idx), range(0, 4)))

    buildid = elf_section(file, ".note.gnu.build-id");
    if buildid is not None:
        # The build ID is an ELF note section, so it begins with a
        # name size (4), a description size (size of contents), a
        # type (3), and the name "GNU\0".
        note_header = buildid[0:16]
        buildid = buildid[16:]
        #print word32(note_header[0:4])
        #print word32(note_header[4:8])
        #print len(buildid)
        #print word32(note_header[8:12])
        if word32(note_header[0:4]) != 4 or \
           word32(note_header[4:8]) != len(buildid) or \
           word32(note_header[8:12]) != 3 or \
           note_header[12:16] != "GNU\0":
            sys.stderr.write("malformed .note.gnu.build_id in " + file + "\n")
        else:
            buildid = "".join(map(lambda ch: "%02X" % ord(ch), buildid)).lower()
            f = os.path.join(global_debug_dir, ".build-id", buildid[0:2], buildid[2:] + ".debug")
            if have_debug_file(f):
                return f

    debuglink = elf_section(file, ".gnu_debuglink");
    if debuglink is not None:
        # The debuglink section contains a string, ending with a
        # null-terminator and then 0 to three bytes of padding to fill the
        # current 32-bit unit.  (This padding is usually null bytes, but
        # I've seen null-null-H, on Ubuntu x86_64.)  This is followed by
        # a 4-byte CRC.
        debuglink_name = debuglink[:-4]
        null_idx = debuglink_name.find("\0")
        if null_idx == -1 or null_idx + 4 < len(debuglink_name):
            sys.stderr.write("Malformed .gnu_debuglink in " + file + "\n")
            return None
        debuglink_name = debuglink_name[0:null_idx]

        debuglink_crc = word32(debuglink[-4:])

        dirname = os.path.dirname(file)
        possible_files = [
            os.path.join(dirname, debuglink_name),
            os.path.join(dirname, ".debug", debuglink_name),
            os.path.join(global_debug_dir, dirname.lstrip("/"), debuglink_name)
        ]
        for f in possible_files:
            if have_debug_file(f):
                # FIXME: Check the CRC!
                return f
    return None

elf_type_re = re.compile("^\s*Type:\s+(\S+)")
elf_text_section_re = re.compile("^\s*\[\s*\d+\]\s+\.text\s+\w+\s+(\w+)\s+(\w+)\s+")

def address_adjustment_for(file):
    """
    Return the address adjustment to use for a file.

    addr2line wants offsets relative to the base address for shared
    libraries, but it wants addresses including the base address offset
    for executables.  This returns the appropriate address adjustment to
    add to an offset within file.  See bug 230336.
    """
    readelf = subprocess.Popen(['readelf', '-h', file],
                               stdout=subprocess.PIPE)
    elftype = None
    for line in readelf.stdout.readlines():
        m = elf_type_re.match(line)
        if m:
            elftype = m.groups()[0]
            break
    readelf.terminate()

    if elftype != "EXEC":
        # If we're not dealing with an executable, return 0.
        return 0

    adjustment = 0
    readelf = subprocess.Popen(['readelf', '-S', file],
                               stdout=subprocess.PIPE)
    for line in readelf.stdout.readlines():
        m = elf_text_section_re.match(line)
        if m:
            # Subtract the .text section's offset within the
            # file from its base address.
            adjustment = int(m.groups()[0], 16) - int(m.groups()[1], 16);
            break
    readelf.terminate()
    return adjustment

addr2lines = {}
def addressToSymbol(file, address):
    converter = None
    address_adjustment = None
    if not file in addr2lines:
        debug_file = separate_debug_file_for(file) or file
        converter = unbufferedLineConverter('/usr/bin/addr2line', ['-C', '-f', '-e', debug_file])
        address_adjustment = address_adjustment_for(file)
        addr2lines[file] = (converter, address_adjustment)
    else:
        (converter, address_adjustment) = addr2lines[file]
    return converter.convert(hex(int(address, 16) + address_adjustment))

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
