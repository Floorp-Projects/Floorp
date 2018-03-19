#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

from conftest import fspath


def test_stackwalk_envvar(check_for_crashes, minidump_files, stackwalk):
    """Test that check_for_crashes uses the MINIDUMP_STACKWALK environment var."""
    os.environ['MINIDUMP_STACKWALK'] = fspath(stackwalk)
    try:
        assert 1 == check_for_crashes(stackwalk_binary=None)
    finally:
        del os.environ['MINIDUMP_STACKWALK']


if __name__ == '__main__':
    mozunit.main()
