# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import errno
import hashlib
import imp
import os
import sys

import manifestdownload
import six
from mach.util import get_state_dir
from mozlog.structured import commandline
from six.moves import configparser
from wptrunner import wptcommandline

manifest = None


def do_delayed_imports(wpt_dir):
    global manifest
    imp.load_source(
        "localpaths", os.path.join(wpt_dir, "tests", "tools", "localpaths.py")
    )
    sys.path.insert(0, os.path.join(wpt_dir, "tools", "manifest"))
    import manifest


def create_parser():
    p = argparse.ArgumentParser()
    p.add_argument(
        "--rebuild", action="store_true", help="Rebuild manifest from scratch"
    )
    download_group = p.add_mutually_exclusive_group()
    download_group.add_argument(
        "--download",
        dest="download",
        action="store_true",
        default=None,
        help="Always download even if the local manifest is recent",
    )
    download_group.add_argument(
        "--no-download",
        dest="download",
        action="store_false",
        help="Don't try to download the manifest",
    )
    p.add_argument(
        "--no-update",
        action="store_false",
        dest="update",
        default=True,
        help="Just download the manifest, don't update",
    )
    p.add_argument(
        "--config",
        action="store",
        dest="config_path",
        default=None,
        help="Path to wptrunner config file",
    )
    p.add_argument(
        "--rewrite-config",
        action="store_true",
        default=False,
        help="Force the local configuration to be regenerated",
    )
    p.add_argument(
        "--cache-root",
        action="store",
        default=os.path.join(get_state_dir(), "cache", "wpt"),
        help="Path to use for the metadata cache",
    )
    commandline.add_logging_group(p)

    return p


def ensure_kwargs(kwargs):
    _kwargs = vars(create_parser().parse_args([]))
    _kwargs.update(kwargs)
    return _kwargs


def run(src_root, obj_root, logger=None, **kwargs):
    kwargs = ensure_kwargs(kwargs)

    if logger is None:
        from wptrunner import wptlogging

        logger = wptlogging.setup(kwargs, {"mach": sys.stdout})

    src_wpt_dir = os.path.join(src_root, "testing", "web-platform")

    do_delayed_imports(src_wpt_dir)

    if not kwargs["config_path"]:
        config_path = generate_config(
            logger,
            src_root,
            src_wpt_dir,
            os.path.join(obj_root, "_tests", "web-platform"),
            kwargs["rewrite_config"],
        )
    else:
        config_path = kwargs["config_path"]

    if not os.path.exists(config_path):
        logger.critical("Config file %s does not exist" % config_path)
        return None

    logger.debug("Using config path %s" % config_path)

    test_paths = wptcommandline.get_test_paths(wptcommandline.config.read(config_path))

    for paths in six.itervalues(test_paths):
        if "manifest_path" not in paths:
            paths["manifest_path"] = os.path.join(
                paths["metadata_path"], "MANIFEST.json"
            )

    ensure_manifest_directories(logger, test_paths)

    local_config = read_local_config(src_wpt_dir)
    for section in ["manifest:upstream", "manifest:mozilla"]:
        url_base = local_config.get(section, "url_base")
        manifest_rel_path = os.path.join(
            local_config.get(section, "metadata"), "MANIFEST.json"
        )
        test_paths[url_base]["manifest_rel_path"] = manifest_rel_path

    if not kwargs["rebuild"] and kwargs["download"] is not False:
        force_download = False if kwargs["download"] is None else True
        manifestdownload.download_from_taskcluster(
            logger, src_root, test_paths, force=force_download
        )
    else:
        logger.debug("Skipping manifest download")

    update = kwargs["update"] or kwargs["rebuild"]
    manifests = load_and_update(
        logger,
        src_wpt_dir,
        test_paths,
        update=update,
        rebuild=kwargs["rebuild"],
        cache_root=kwargs["cache_root"],
    )

    return manifests


def ensure_manifest_directories(logger, test_paths):
    for paths in six.itervalues(test_paths):
        manifest_dir = os.path.dirname(paths["manifest_path"])
        if not os.path.exists(manifest_dir):
            logger.info("Creating directory %s" % manifest_dir)
            # Even though we just checked the path doesn't exist, there's a chance
            # of race condition with another process or thread having created it in
            # between. This happens during tests.
            try:
                os.makedirs(manifest_dir)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise
        elif not os.path.isdir(manifest_dir):
            raise IOError("Manifest directory is a file")


def read_local_config(wpt_dir):
    src_config_path = os.path.join(wpt_dir, "wptrunner.ini")

    parser = configparser.SafeConfigParser()
    success = parser.read(src_config_path)
    assert src_config_path in success
    return parser


def generate_config(logger, repo_root, wpt_dir, dest_path, force_rewrite=False):
    """Generate the local wptrunner.ini file to use locally"""
    if not os.path.exists(dest_path):
        # Even though we just checked the path doesn't exist, there's a chance
        # of race condition with another process or thread having created it in
        # between. This happens during tests.
        try:
            os.makedirs(dest_path)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    dest_config_path = os.path.join(dest_path, "wptrunner.local.ini")

    if not force_rewrite and os.path.exists(dest_config_path):
        logger.debug("Config is up to date, not regenerating")
        return dest_config_path

    logger.info("Creating config file %s" % dest_config_path)

    parser = read_local_config(wpt_dir)

    for section in ["manifest:upstream", "manifest:mozilla"]:
        meta_rel_path = parser.get(section, "metadata")
        tests_rel_path = parser.get(section, "tests")

        parser.set(
            section, "manifest", os.path.join(dest_path, meta_rel_path, "MANIFEST.json")
        )
        parser.set(section, "metadata", os.path.join(wpt_dir, meta_rel_path))
        parser.set(section, "tests", os.path.join(wpt_dir, tests_rel_path))

    parser.set(
        "paths",
        "prefs",
        os.path.abspath(os.path.join(wpt_dir, parser.get("paths", "prefs"))),
    )

    with open(dest_config_path, "wt") as config_file:
        parser.write(config_file)

    return dest_config_path


def load_and_update(
    logger,
    wpt_dir,
    test_paths,
    rebuild=False,
    config_dir=None,
    cache_root=None,
    update=True,
):
    rv = {}
    wptdir_hash = hashlib.sha256(os.path.abspath(wpt_dir).encode()).hexdigest()
    for url_base, paths in six.iteritems(test_paths):
        manifest_path = paths["manifest_path"]
        this_cache_root = os.path.join(
            cache_root, wptdir_hash, os.path.dirname(paths["manifest_rel_path"])
        )
        m = manifest.manifest.load_and_update(
            paths["tests_path"],
            manifest_path,
            url_base,
            update=update,
            rebuild=rebuild,
            working_copy=True,
            cache_root=this_cache_root,
        )
        path_data = {"url_base": url_base}
        path_data.update(paths)
        rv[m] = path_data

    return rv


def log_error(logger, manifest_path, msg):
    logger.lint_error(
        path=manifest_path, message=msg, lineno=0, source="", linter="wpt-manifest"
    )
