#!/usr/bin/env python

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import zipfile


class ProcError(Exception):
    def __init__(self, returncode, stderr):
        self.returncode = returncode
        self.stderr = stderr


def check_output(command):
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    if proc.returncode != 0:
        raise ProcError(proc.returncode, stderr)
    return stdout


def process_file(dump_syms, path):
    try:
        stdout = check_output([dump_syms, path])
    except ProcError as e:
        print('Error: running "%s %s": %s' % (dump_syms, path, e.stderr))
        return None, None, None
    bits = stdout.splitlines()[0].split(' ', 4)
    if len(bits) != 5:
        return None, None, None
    _, platform, cpu_arch, debug_id, debug_file = bits
    if debug_file.lower().endswith('.pdb'):
        sym_file = debug_file[:-4] + '.sym'
    else:
        sym_file = debug_file + '.sym'
    filename = os.path.join(debug_file, debug_id, sym_file)
    debug_filename = os.path.join(debug_file, debug_id, debug_file)
    return filename, stdout, debug_filename


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('dump_syms', help='Path to dump_syms binary')
    parser.add_argument('files', nargs='+',
                        help='Path to files to dump symbols from')
    parser.add_argument('--symbol-zip', default='symbols.zip',
                        help='Name of zip file to put dumped symbols in')
    parser.add_argument('--no-binaries',
                        action='store_true',
                        default=False,
                        help='Don\'t store binaries in zip file')
    args = parser.parse_args()
    count = 0
    with zipfile.ZipFile(args.symbol_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for f in args.files:
            filename, contents, debug_filename = process_file(args.dump_syms, f)
            if not (filename and contents):
                print('Error dumping symbols')
                sys.exit(1)
            zf.writestr(filename, contents)
            count += 1
            if not args.no_binaries:
                zf.write(f, debug_filename)
                count += 1
    print('Added %d files to %s' % (count, args.symbol_zip))

if __name__ == '__main__':
    main()
