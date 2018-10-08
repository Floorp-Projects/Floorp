# -*- coding=utf-8 -*-
import importlib

def do_import(module_path, subimport=None, old_path=None):
    old_path = old_path or module_path
    prefixes = ["pip._internal", "pip"]
    paths = [module_path, old_path]
    search_order = ["{0}.{1}".format(p, pth) for p in prefixes for pth in paths if pth is not None]
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


InstallRequirement = do_import('req.req_install', 'InstallRequirement')
parse_requirements = do_import('req.req_file', 'parse_requirements')
RequirementSet = do_import('req.req_set', 'RequirementSet')
user_cache_dir = do_import('utils.appdirs', 'user_cache_dir')
FAVORITE_HASH = do_import('utils.hashes', 'FAVORITE_HASH')
is_file_url = do_import('download', 'is_file_url')
url_to_path = do_import('download', 'url_to_path')
PackageFinder = do_import('index', 'PackageFinder')
FormatControl = do_import('index', 'FormatControl')
Wheel = do_import('wheel', 'Wheel')
Command = do_import('cli.base_command', 'Command', old_path='basecommand')
cmdoptions = do_import('cli.cmdoptions', old_path='cmdoptions')
get_installed_distributions = do_import('utils.misc', 'get_installed_distributions', old_path='utils')
PyPI = do_import('models.index', 'PyPI')
