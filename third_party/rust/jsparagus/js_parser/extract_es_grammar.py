"""extract_es_grammar.py - Extract the grammar from the ECMAScript spec

To run this script, you first need to get the source of the version of
the ECMAScript spec you're interested in.

    cd ../..
    mkdir tc39
    cd tc39
    git clone git@github.com:tc39/ecma262.git

Then:

    make js_parser/es.esgrammar

You can also use this script on a random HTTPS URL, like:

    python extract_esgrammar.py https://raw.githubusercontent.com/tc39/proposal-class-fields/master/spec.html

"""

import argparse
import urllib
import html5lib
import re
from textwrap import dedent


HTML = "{http://www.w3.org/1999/xhtml}"
INS_TAG = HTML + "ins"
DEL_TAG = HTML + "del"

INS = '+'
DEL = '-'
KEEP = ' '


def fragment_chunks(e):
    """Partly interpret the content of `e`, yielding pairs (ty, text).

    The content of `e` must be text with 0 or more <ins>/<del> elements.
    The goal is to turn the tree `e` into a simple series of tagged strings.

    Yields pairs (ty, text) where ty in (INS, DEL, KEEP). Concatenating the
    yielded `text` values gives the full text of `e`.
    """

    if e.text:
        yield KEEP, e.text
    for child in e:
        if child.tag == INS_TAG:
            ty = INS
        elif child.tag == DEL_TAG:
            ty = DEL
        else:
            raise ValueError("unrecognized element: " + child.tag)

        if child.text:
            yield ty, child.text
        if len(child) != 0:
            raise ValueError("unsupported nested elements in " + child.tag)
        if child.tail:
            yield KEEP, child.tail


def fragment_parts(e):
    """Like fragment_chunks, but with two fixups.

    1.  Break up pairs that include both a newline and any other text.

    2.  Move newlines inside of a preceding INS or DEL element that spans its
        whole line.
    """
    line_has_ins = False
    line_has_del = False
    for chunk_ty, text in fragment_chunks(e):
        for piece in re.split(r'(\n)', text):
            ty = chunk_ty
            if piece != '':
                if piece == '\n':
                    # Possibly move newline inside preceding INS or DEL.
                    if line_has_ins and not line_has_del:
                        ty = INS
                    elif line_has_del and not line_has_ins:
                        ty = DEL
                    else:
                        ty = KEEP
                    line_has_del = False
                    line_has_ins = False
                elif piece.strip() != '':
                    if ty in (INS, KEEP):
                        line_has_ins = True
                    if ty in (DEL, KEEP):
                        line_has_del = True
                yield ty, piece


def generate_fragment_patch(e):
    line_before = ''
    line_after = ''

    def end_line(ty):
        nonlocal line_before, line_after
        if line_before.rstrip() == line_after.rstrip():
            yield " ", line_after
        else:
            if line_before.strip() != '' or ty != INS:
                yield "-", line_before
            if line_after.strip() != '' or ty != DEL:
                yield "+", line_after
        line_before = ''
        line_after = ''

    for ty, text in fragment_parts(e):
        if text == '\n':
            yield from end_line(ty)
        else:
            if ty in (KEEP, DEL):
                line_before += text
            if ty in (KEEP, INS):
                line_after += text
    if line_before or line_after:
        yield from end_line(KEEP)

def print_fragment_patch(e):
    pairs = list(generate_fragment_patch(e))

    # Using textwrap.dedent on this requires a few lines of hackery.
    types = [ty for ty, _line in pairs]
    dedented_lines = dedent(''.join(line + '\n' for ty, line in pairs)).splitlines()
    assert len(dedented_lines) == len(pairs)

    for ty, line in zip(types, dedented_lines):
        print(ty + line)


def extract(filename, unfiltered):
    if filename.startswith("https:"):
        file_obj = urllib.request.urlopen(filename)
    else:
        file_obj = open(filename, "rb")

    with file_obj:
        document = html5lib.parse(file_obj)

    for e in document.iter("{http://www.w3.org/1999/xhtml}emu-grammar"):
        if unfiltered or e.attrib.get("type") == "definition":
            print_fragment_patch(e)


if __name__ == '__main__':
    import sys
    parser = argparse.ArgumentParser(
        description="Extract esgrammar from ECMAScript specifications.")
    parser.add_argument(
        'url',
        nargs=1,
        help="the https: url or local filename of an HTML file containing <emu-grammar> tags")
    parser.add_argument(
        '--unfiltered',
        action='store_true',
        help="Include even <emu-grammar> elements that don't have `type=definition`")

    args = parser.parse_args()
    extract(args.url[0], args.unfiltered)
