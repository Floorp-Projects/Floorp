# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import pytest
from responses import RequestsMock


@pytest.fixture
def responses():
    with RequestsMock() as rsps:
        yield rsps
