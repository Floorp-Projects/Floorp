#!/usr/bin/env python3
"""
Upload CHANGES.md to Tidelift as Markdown chunks

Put your Tidelift API token in a file called tidelift.token alongside this
program, for example:

    user/n3IwOpxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxc2ZwE4

Run with two arguments: the .md file to parse, and the Tidelift package name:

	python upload_relnotes.py CHANGES.md pypi/coverage

Every section that has something that looks like a version number in it will
be uploaded as the release notes for that version.

"""

import os.path
import re
import sys

import requests

class TextChunkBuffer:
    """Hold onto text chunks until needed."""
    def __init__(self):
        self.buffer = []

    def append(self, text):
        """Add `text` to the buffer."""
        self.buffer.append(text)

    def clear(self):
        """Clear the buffer."""
        self.buffer = []

    def flush(self):
        """Produce a ("text", text) tuple if there's anything here."""
        buffered = "".join(self.buffer).strip()
        if buffered:
            yield ("text", buffered)
        self.clear()


def parse_md(lines):
    """Parse markdown lines, producing (type, text) chunks."""
    buffer = TextChunkBuffer()

    for line in lines:
        header_match = re.search(r"^(#+) (.+)$", line)
        is_header = bool(header_match)
        if is_header:
            yield from buffer.flush()
            hashes, text = header_match.groups()
            yield (f"h{len(hashes)}", text)
        else:
            buffer.append(line)

    yield from buffer.flush()


def sections(parsed_data):
    """Convert a stream of parsed tokens into sections with text and notes.

    Yields a stream of:
        ('h-level', 'header text', 'text')

    """
    header = None
    text = []
    for ttype, ttext in parsed_data:
        if ttype.startswith('h'):
            if header:
                yield (*header, "\n".join(text))
            text = []
            header = (ttype, ttext)
        elif ttype == "text":
            text.append(ttext)
        else:
            raise Exception(f"Don't know ttype {ttype!r}")
    yield (*header, "\n".join(text))


def relnotes(mdlines):
    r"""Yield (version, text) pairs from markdown lines.

    Each tuple is a separate version mentioned in the release notes.

    A version is any section with \d\.\d in the header text.

    """
    for _, htext, text in sections(parse_md(mdlines)):
        m_version = re.search(r"\d+\.\d[^ ]*", htext)
        if m_version:
            version = m_version.group()
            yield version, text

def update_release_note(package, version, text):
    """Update the release notes for one version of a package."""
    url = f"https://api.tidelift.com/external-api/lifting/{package}/release-notes/{version}"
    token_file = os.path.join(os.path.dirname(__file__), "tidelift.token")
    with open(token_file) as ftoken:
        token = ftoken.read().strip()
    headers = {
        "Authorization": f"Bearer: {token}",
    }
    req_args = dict(url=url, data=text.encode('utf8'), headers=headers)
    result = requests.post(**req_args)
    if result.status_code == 409:
        result = requests.put(**req_args)
    print(f"{version}: {result.status_code}")

def parse_and_upload(md_filename, package):
    """Main function: parse markdown and upload to Tidelift."""
    with open(md_filename) as f:
        markdown = f.read()
    for version, text in relnotes(markdown.splitlines(True)):
        update_release_note(package, version, text)

if __name__ == "__main__":
    parse_and_upload(*sys.argv[1:])       # pylint: disable=no-value-for-parameter
