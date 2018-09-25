import argparse
import imp
import os
import sys
from collections import defaultdict

from mozlog.structured import commandline
from wptrunner.wptcommandline import get_test_paths, set_from_config

manifest = None


def do_delayed_imports(wpt_dir):
    global manifest
    sys.path.insert(0, os.path.join(wpt_dir, "tools", "manifest"))
    import manifest



def create_parser():
    p = argparse.ArgumentParser()
    p.add_argument("--rebuild", action="store_true",
                   help="Rebuild the manifest from scratch")
    commandline.add_logging_group(p)

    return p


def update(logger, wpt_dir, rebuild=False, config_dir=None):
    localpaths = imp.load_source("localpaths",
                                 os.path.join(wpt_dir, "tests", "tools", "localpaths.py"))

    if not config_dir:
        config_dir = wpt_dir
        config_name = "wptrunner.ini"
    else:
        if not os.path.exists(os.path.join(config_dir, 'wptrunner.local.ini')):
            from manifestdownload import generate_config
            generate_config(config_dir)
        config_name = "wptrunner.local.ini"

    kwargs = {"config": os.path.join(config_dir, config_name),
              "tests_root": None,
              "metadata_root": None}

    set_from_config(kwargs)
    do_delayed_imports(wpt_dir)

    return _update(logger, kwargs["test_paths"], rebuild)


def _update(logger, test_paths, rebuild):
    for url_base, paths in test_paths.iteritems():
        if "manifest_path" in paths:
            manifest_path = paths["manifest_path"]
        else:
            manifest_path = os.path.join(paths["metadata_path"], "MANIFEST.json")
        m = None
        if not rebuild and os.path.exists(manifest_path):
            try:
                m = manifest.manifest.load(paths["tests_path"], manifest_path)
            except manifest.manifest.ManifestVersionMismatch:
                logger.info("Manifest format changed, rebuilding")
        if m is None:
            m = manifest.manifest.Manifest(url_base)
        manifest.update.update(paths["tests_path"], m, working_copy=True)
        manifest.manifest.write(m, manifest_path)
    return 0


def log_error(logger, manifest_path, msg):
    logger.lint_error(path=manifest_path,
                      message=msg,
                      lineno=0,
                      source="",
                      linter="wpt-manifest")
