#!/usr/bin/env python

"""
test .ini parsing

ensure our .ini parser is doing what we want; to be deprecated for
python's standard ConfigParser when 2.7 is reality so OrderedDict
is the default:

http://docs.python.org/2/library/configparser.html
"""

from __future__ import absolute_import

from textwrap import dedent

import mozunit
import pytest
from six import StringIO

from manifestparser import read_ini


@pytest.fixture(scope='module')
def parse_manifest():

    def inner(string, **kwargs):
        buf = StringIO()
        buf.write(dedent(string))
        buf.seek(0)
        return read_ini(buf, **kwargs)[0]

    return inner


def test_inline_comments(parse_manifest):
    result = parse_manifest("""
    [test_felinicity.py]
    kittens = true # This test requires kittens
    cats = false#but not cats
    """)[0][1]

    # make sure inline comments get stripped out, but comments without a space in front don't
    assert result['kittens'] == 'true'
    assert result['cats'] == "false#but not cats"


def test_line_continuation(parse_manifest):
    result = parse_manifest("""
    [test_caninicity.py]
    breeds =
      sheppard
      retriever
      terrier

    [test_cats_and_dogs.py]
      cats=yep
      dogs=
        yep
          yep
    birds=nope
      fish=nope
    """)
    assert result[0][1]['breeds'].split() == ['sheppard', 'retriever', 'terrier']
    assert result[1][1]['cats'] == 'yep'
    assert result[1][1]['dogs'].split() == ['yep', 'yep']
    assert result[1][1]['birds'].split() == ['nope', 'fish=nope']


def test_dupes_error(parse_manifest):
    dupes = """
    [test_dupes.py]
    foo = bar
    foo = baz
    """
    with pytest.raises(AssertionError):
        parse_manifest(dupes, strict=True)

    with pytest.raises(AssertionError):
        parse_manifest(dupes, strict=False)


def test_defaults_handling(parse_manifest):
    manifest = """
    [DEFAULT]
    flower = rose
    skip-if = true

    [test_defaults]
    """

    result = parse_manifest(manifest)[0][1]
    assert result['flower'] == 'rose'
    assert result['skip-if'] == 'true'

    result = parse_manifest(
        manifest,
        defaults={
            'flower': 'tulip',
            'colour': 'pink',
            'skip-if': 'false',
        },
    )[0][1]
    assert result['flower'] == 'rose'
    assert result['colour'] == 'pink'
    assert result['skip-if'] == '(false) || (true)'

    result = parse_manifest(manifest.replace('DEFAULT', 'default'))[0][1]
    assert result['flower'] == 'rose'
    assert result['skip-if'] == 'true'


if __name__ == '__main__':
    mozunit.main()
