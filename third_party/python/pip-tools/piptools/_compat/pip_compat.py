# -*- coding=utf-8 -*-
from __future__ import absolute_import

import importlib
from contextlib import contextmanager

import pip
from pip._vendor.packaging.version import parse as parse_version

PIP_VERSION = tuple(map(int, parse_version(pip.__version__).base_version.split(".")))

try:
    from pip._internal.req.req_tracker import RequirementTracker
except ImportError:

    @contextmanager
    def RequirementTracker():
        yield


# Introduced in pip 20.0
try:
    from pip._internal.utils.temp_dir import global_tempdir_manager
except ImportError:

    @contextmanager
    def global_tempdir_manager():
        yield


def do_import(module_path, subimport=None, old_path=None):
    old_path = old_path or module_path
    prefixes = ["pip._internal", "pip"]
    paths = [module_path, old_path]
    search_order = [
        "{0}.{1}".format(p, pth) for p in prefixes for pth in paths if pth is not None
    ]
    package = subimport if subimport else None
    for to_import in search_order:
        if not subimport:
            to_import, _, package = to_import.rpartition(".")
        try:
            imported = importlib.import_module(to_import)
        except ImportError:
            continue
        else:
            return getattr(imported, package)


InstallRequirement = do_import("req.req_install", "InstallRequirement")
InstallationCandidate = do_import(
    "models.candidate", "InstallationCandidate", old_path="index"
)
parse_requirements = do_import("req.req_file", "parse_requirements")
RequirementSet = do_import("req.req_set", "RequirementSet")
user_cache_dir = do_import("utils.appdirs", "user_cache_dir")
FAVORITE_HASH = do_import("utils.hashes", "FAVORITE_HASH")
path_to_url = do_import("utils.urls", "path_to_url", old_path="download")
url_to_path = do_import("utils.urls", "url_to_path", old_path="download")
PackageFinder = do_import("index.package_finder", "PackageFinder", old_path="index")
FormatControl = do_import("models.format_control", "FormatControl", old_path="index")
InstallCommand = do_import("commands.install", "InstallCommand")
Wheel = do_import("models.wheel", "Wheel", old_path="wheel")
cmdoptions = do_import("cli.cmdoptions", old_path="cmdoptions")
get_installed_distributions = do_import(
    "utils.misc", "get_installed_distributions", old_path="utils"
)
PyPI = do_import("models.index", "PyPI")
stdlib_pkgs = do_import("utils.compat", "stdlib_pkgs", old_path="compat")
DEV_PKGS = do_import("commands.freeze", "DEV_PKGS")
Link = do_import("models.link", "Link", old_path="index")
Session = do_import("_vendor.requests.sessions", "Session")
Resolver = do_import("legacy_resolve", "Resolver", old_path="resolve")
WheelCache = do_import("cache", "WheelCache", old_path="wheel")
normalize_path = do_import("utils.misc", "normalize_path", old_path="utils")

# pip 18.1 has refactored InstallRequirement constructors use by pip-tools.
if PIP_VERSION < (18, 1):
    install_req_from_line = InstallRequirement.from_line
    install_req_from_editable = InstallRequirement.from_editable
else:
    install_req_from_line = do_import("req.constructors", "install_req_from_line")
    install_req_from_editable = do_import(
        "req.constructors", "install_req_from_editable"
    )


def is_vcs_url(link):
    if PIP_VERSION < (19, 3):
        _is_vcs_url = do_import("download", "is_vcs_url")
        return _is_vcs_url(link)

    return link.is_vcs


def is_file_url(link):
    if PIP_VERSION < (19, 3):
        _is_file_url = do_import("download", "is_file_url")
        return _is_file_url(link)

    return link.is_file


def is_dir_url(link):
    if PIP_VERSION < (19, 3):
        _is_dir_url = do_import("download", "is_dir_url")
        return _is_dir_url(link)

    return link.is_existing_dir()


def get_requirement_tracker():
    if PIP_VERSION[:2] <= (19, 3):
        return RequirementTracker()

    from pip._internal.req import req_tracker

    return req_tracker.get_requirement_tracker()
