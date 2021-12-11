# -*- coding=utf-8 -*-
from __future__ import absolute_import

import pip
from pip._internal.req import parse_requirements as _parse_requirements
from pip._internal.req.constructors import install_req_from_parsed_requirement
from pip._vendor.packaging.version import parse as parse_version

PIP_VERSION = tuple(map(int, parse_version(pip.__version__).base_version.split(".")))


def parse_requirements(
    filename, session, finder=None, options=None, constraint=False, isolated=False
):
    for parsed_req in _parse_requirements(
        filename, session, finder=finder, options=options, constraint=constraint
    ):
        yield install_req_from_parsed_requirement(parsed_req, isolated=isolated)
