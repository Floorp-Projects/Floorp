# coding: utf-8
# flake8: noqa
from __future__ import absolute_import, division, print_function, unicode_literals

import six

from .pip_compat import BAR_TYPES, PIP_VERSION, parse_requirements

if six.PY2:
    from .tempfile import TemporaryDirectory
else:
    from tempfile import TemporaryDirectory
