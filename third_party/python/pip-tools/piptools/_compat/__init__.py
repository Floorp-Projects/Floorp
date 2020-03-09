# coding: utf-8
# flake8: noqa
from __future__ import absolute_import, division, print_function, unicode_literals

import six

from .pip_compat import (
    DEV_PKGS,
    FAVORITE_HASH,
    PIP_VERSION,
    FormatControl,
    InstallationCandidate,
    InstallCommand,
    InstallRequirement,
    Link,
    PackageFinder,
    PyPI,
    RequirementSet,
    Resolver,
    Wheel,
    WheelCache,
    cmdoptions,
    get_installed_distributions,
    get_requirement_tracker,
    global_tempdir_manager,
    install_req_from_editable,
    install_req_from_line,
    is_dir_url,
    is_file_url,
    is_vcs_url,
    normalize_path,
    parse_requirements,
    path_to_url,
    stdlib_pkgs,
    url_to_path,
    user_cache_dir,
)

if six.PY2:
    from .tempfile import TemporaryDirectory
else:
    from tempfile import TemporaryDirectory
