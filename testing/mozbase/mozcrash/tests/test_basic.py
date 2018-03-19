#!/usr/bin/env python

from __future__ import absolute_import

import mozunit
import pytest


def test_no_dump_files(check_for_crashes):
    """Test that check_for_crashes returns 0 if no dumps are present."""
    assert 0 == check_for_crashes()


@pytest.mark.parametrize('minidump_files', [3], indirect=True)
def test_dump_count(check_for_crashes, minidump_files):
    """Test that check_for_crashes returns the number of crash dumps."""
    assert 3 == check_for_crashes()


if __name__ == '__main__':
    mozunit.main()
