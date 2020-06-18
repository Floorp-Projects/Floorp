# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

"""Find lines in files that should be faithful copies, and check that they are.

Inside a comment-marked section, any chunk of indented lines should be
faithfully copied from FILENAME.  The indented lines are dedented before
comparing.

The section is between these comments:

    .. copied_from <FILENAME>

    .. end_copied_from

This tool will print any mismatches, and then exit with a count of mismatches.

"""

import glob
from itertools import groupby
from operator import itemgetter
import re
import sys
import textwrap


def check_copied_from(rst_name):
    """Check copies in a .rst file.

    Prints problems.  Returns count of bad copies.
    """
    bad_copies = 0
    file_read = None
    file_text = None
    with open(rst_name) as frst:
        for filename, first_line, text in find_copied_chunks(frst):
            if filename != file_read:
                with open(filename) as f:
                    file_text = f.read()
                file_read = filename
            if text not in file_text:
                print("{}:{}: Bad copy from {}, starting with {!r}".format(
                    rst_name, first_line, filename, text.splitlines()[0]
                ))
                bad_copies += 1

    return bad_copies


def find_copied_chunks(frst):
    """Find chunks of text that are meant to be faithful copies.

    `frst` is an iterable of strings, the .rst text.

    Yields (source_filename, first_line, text) tuples.
    """
    for (_, filename), chunks in groupby(find_copied_lines(frst), itemgetter(0)):
        chunks = list(chunks)
        first_line = chunks[0][1]
        text = textwrap.dedent("\n".join(map(itemgetter(2), chunks)))
        yield filename, first_line, text


def find_copied_lines(frst):
    """Find lines of text that are meant to be faithful copies.

    `frst` is an iterable of strings, the .rst text.

    Yields tuples ((chunk_num, file_name), line_num, line).

    `chunk_num` is an integer that is different for each distinct (blank
    line separated) chunk of text, but has no meaning other than that.

    `file_name` is the file the chunk should be copied from.  `line_num`
    is the line number in the .rst file, and `line` is the text of the line.

    """
    in_section = False
    source_file = None
    chunk_num = 0

    for line_num, line in enumerate(frst, start=1):
        line = line.rstrip()
        if in_section:
            m = re.search(r"^.. end_copied_from", line)
            if m:
                in_section = False
            else:
                if re.search(r"^\s+\S", line):
                    # Indented line
                    yield (chunk_num, source_file), line_num, line
                elif not line.strip():
                    # Blank line
                    chunk_num += 1
        else:
            m = re.search(r"^.. copied_from: (.*)", line)
            if m:
                in_section = True
                source_file = m.group(1)


def main(args):
    """Check all the files in `args`, return count of bad copies."""
    bad_copies = 0
    for arg in args:
        for fname in glob.glob(arg):
            bad_copies += check_copied_from(fname)
    return bad_copies


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
