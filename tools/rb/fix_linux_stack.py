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

# Table of 256 values, per documentation of .gnu_debuglink sections.
gnu_debuglink_crc32_table = [
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
    0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
    0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
    0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
    0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
    0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
    0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
    0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
    0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
    0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
    0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
    0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
    0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
    0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
    0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
    0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
    0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
    0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
    0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
    0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
    0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
    0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
    0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
    0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
    0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
    0x2d02ef8d
]

def gnu_debuglink_crc32(stream):
    # Note that treats bitwise operators as though integers have an
    # infinite number of bits (and thus such that negative integers
    # 1-pad out to infinity).
    crc = 0xffffffff
    while True:
        # Choose to read in 4096 byte chunks.
        bytes = stream.read(4096)
        if len(bytes) == 0:
            break
        for byte in bytes:
            crc = gnu_debuglink_crc32_table[(crc ^ ord(byte)) & 0xff] ^ (crc >> 8)
    return ~crc & 0xffffffff

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
                fio = open(f, mode="r")
                file_crc = gnu_debuglink_crc32(fio)
                fio.close()
                if file_crc == debuglink_crc:
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
    cache = None
    if not file in addr2lines:
        debug_file = separate_debug_file_for(file) or file
        converter = unbufferedLineConverter('/usr/bin/addr2line', ['-C', '-f', '-e', debug_file])
        address_adjustment = address_adjustment_for(file)
        cache = {}
        addr2lines[file] = (converter, address_adjustment, cache)
    else:
        (converter, address_adjustment, cache) = addr2lines[file]
    if address in cache:
        return cache[address]
    result = converter.convert(hex(int(address, 16) + address_adjustment))
    cache[address] = result
    return result

line_re = re.compile("^(.*) ?\[([^ ]*) \+(0x[0-9A-F]{1,8})\](.*)$")
balance_tree_re = re.compile("^([ \|0-9-]*)(.*)$")

def fixSymbols(line):
    result = line_re.match(line)
    if result is not None:
        # before allows preservation of balance trees
        # after allows preservation of counts
        (before, file, address, after) = result.groups()

        if os.path.exists(file) and os.path.isfile(file):
            # throw away the bad symbol, but keep balance tree structure
            (before, badsymbol) = balance_tree_re.match(before).groups()

            (name, fileline) = addressToSymbol(file, address)

            # If addr2line gave us something useless, keep what we had before.
            if name == "??":
                name = badsymbol
            if fileline == "??:0" or fileline == "??:?":
                fileline = file

            return "%s%s (%s)%s\n" % (before, name, fileline, after)
        else:
            sys.stderr.write("Warning: File \"" + file + "\" does not exist.\n")
            return line
    else:
        return line

if __name__ == "__main__":
    for line in sys.stdin:
        sys.stdout.write(fixSymbols(line))
