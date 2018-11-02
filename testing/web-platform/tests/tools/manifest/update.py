#!/usr/bin/env python
import argparse
import os

import manifest
from . import vcs
from .log import get_logger
from .download import download_from_github

here = os.path.dirname(__file__)

wpt_root = os.path.abspath(os.path.join(here, os.pardir, os.pardir))

logger = get_logger()


def update(tests_root, manifest, working_copy=False, cache_root=None, rebuild=False):
    logger.info("Updating manifest")
    tree = None
    if cache_root is None:
        cache_root = os.path.join(tests_root, ".cache")
    if not os.path.exists(cache_root):
        try:
            os.makedirs(cache_root)
        except IOError:
            cache_root = None

    if not working_copy:
        tree = vcs.Git.for_path(tests_root, manifest.url_base,
                                cache_path=cache_root, rebuild=rebuild)
    if tree is None:
        tree = vcs.FileSystem(tests_root, manifest.url_base,
                              cache_path=cache_root, rebuild=rebuild)

    try:
        return manifest.update(tree)
    finally:
        tree.dump_caches()


def update_from_cli(**kwargs):
    tests_root = kwargs["tests_root"]
    path = kwargs["path"]
    assert tests_root is not None

    m = None

    if kwargs["download"]:
        download_from_github(path, tests_root)

    if not kwargs.get("rebuild", False):
        try:
            m = manifest.load(tests_root, path)
        except manifest.ManifestVersionMismatch:
            logger.info("Manifest version changed, rebuilding")
            m = None

    if m is None:
        m = manifest.Manifest(kwargs["url_base"])

    changed = update(tests_root,
                     m,
                     working_copy=kwargs["work"],
                     cache_root=kwargs["cache_root"],
                     rebuild=kwargs["rebuild"])
    if changed:
        manifest.write(m, path)


def abs_path(path):
    return os.path.abspath(os.path.expanduser(path))


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--path", type=abs_path, help="Path to manifest file.")
    parser.add_argument(
        "--tests-root", type=abs_path, default=wpt_root, help="Path to root of tests.")
    parser.add_argument(
        "-r", "--rebuild", action="store_true", default=False,
        help="Force a full rebuild of the manifest.")
    parser.add_argument(
        "--work", action="store_true", default=False,
        help="Build from the working tree rather than the latest commit")
    parser.add_argument(
        "--url-base", action="store", default="/",
        help="Base url to use as the mount point for tests in this manifest.")
    parser.add_argument(
        "--no-download", dest="download", action="store_false", default=True,
        help="Never attempt to download the manifest.")
    parser.add_argument(
        "--cache-root", action="store", default=os.path.join(wpt_root, ".wptcache"),
        help="Path in which to store any caches (default <tests_root>/.wptcache/")
    return parser


def find_top_repo():
    path = here
    rv = None
    while path != "/":
        if vcs.is_git_repo(path):
            rv = path
        path = os.path.abspath(os.path.join(path, os.pardir))

    return rv


def run(*args, **kwargs):
    if kwargs["path"] is None:
        kwargs["path"] = os.path.join(kwargs["tests_root"], "MANIFEST.json")
    update_from_cli(**kwargs)


def main():
    opts = create_parser().parse_args()

    run(**vars(opts))
