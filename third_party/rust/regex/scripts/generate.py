#!/usr/bin/env python3

# This uses `ucd-generate` to generate all of the internal tables.

import argparse
import os
import subprocess
import sys


def eprint(*args, **kwargs):
    kwargs['file'] = sys.stderr
    print(*args, **kwargs)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('ucd', metavar='DIR', nargs=1)
    args = p.parse_args()
    ucd = args.ucd[0]

    def generate(subcmd, *args, **kwargs):
        subprocess.run(("ucd-generate", subcmd, ucd) + args, check=True, **kwargs)

    def generate_file(path, subcmd, *args, filename=None, **kwargs):
        if filename is None:
            filename = subcmd.replace('-', '_') + ".rs"
        eprint('-', filename)
        with open(os.path.join(path, filename), "w") as f:
            generate(subcmd, *args, stdout=f, **kwargs)

    eprint('generating regex-syntax tables')
    path = os.path.join("regex-syntax", "src", "unicode_tables")
    generate_file(path, "age", "--chars")
    generate_file(path, "case-folding-simple", "--chars", "--all-pairs")
    generate_file(path, "general-category", "--chars", "--exclude", "surrogate")
    generate_file(path, "perl-word", "--chars")
    generate_file(path, "property-bool", "--chars")
    generate_file(path, "property-names")
    generate_file(path, "property-values", "--include", "gc,script,scx,age")
    generate_file(path, "script", "--chars")
    generate_file(path, "script-extension", "--chars")

if __name__ == '__main__':
    main()
