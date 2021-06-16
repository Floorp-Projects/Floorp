# coding: utf-8
# flake8: noqa
from __future__ import absolute_import, division, print_function, unicode_literals

import errno
import os

from pip._vendor import six

from .pip_compat import PIP_VERSION, parse_requirements

if six.PY2:
    from .tempfile import TemporaryDirectory
else:
    from tempfile import TemporaryDirectory


def makedirs(name, mode=0o777, exist_ok=False):
    if six.PY2:
        try:
            os.makedirs(name, mode)
        except OSError as e:
            if not exist_ok or e.errno != errno.EEXIST:
                raise
    else:
        os.makedirs(name, mode, exist_ok)
