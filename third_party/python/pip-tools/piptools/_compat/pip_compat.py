# -*- coding=utf-8 -*-
import importlib

import pip
import pkg_resources


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
is_file_url = do_import("download", "is_file_url")
is_dir_url = do_import("download", "is_dir_url")
is_vcs_url = do_import("download", "is_vcs_url")
path_to_url = do_import("download", "path_to_url")
url_to_path = do_import("download", "url_to_path")
PackageFinder = do_import("index", "PackageFinder")
FormatControl = do_import("index", "FormatControl")
Wheel = do_import("wheel", "Wheel")
Command = do_import("cli.base_command", "Command", old_path="basecommand")
cmdoptions = do_import("cli.cmdoptions", old_path="cmdoptions")
get_installed_distributions = do_import(
    "utils.misc", "get_installed_distributions", old_path="utils"
)
PyPI = do_import("models.index", "PyPI")
stdlib_pkgs = do_import("utils.compat", "stdlib_pkgs", old_path="compat")
DEV_PKGS = do_import("commands.freeze", "DEV_PKGS")
Link = do_import("models.link", "Link", old_path="index")
Session = do_import("_vendor.requests.sessions", "Session")

# pip 18.1 has refactored InstallRequirement constructors use by pip-tools.
if pkg_resources.parse_version(pip.__version__) < pkg_resources.parse_version("18.1"):
    install_req_from_line = InstallRequirement.from_line
    install_req_from_editable = InstallRequirement.from_editable
else:
    install_req_from_line = do_import("req.constructors", "install_req_from_line")
    install_req_from_editable = do_import(
        "req.constructors", "install_req_from_editable"
    )
