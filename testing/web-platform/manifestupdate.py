import argparse
import imp
import os
import sys

from mozlog.structured import commandline
from wptrunner.wptcommandline import get_test_paths, set_from_config
from wptrunner.testloader import ManifestLoader

def create_parser():
    p = argparse.ArgumentParser()
    p.add_argument("--check-clean", action="store_true",
                   help="Check that updating the manifest doesn't lead to any changes")
    commandline.add_logging_group(p)

    return p


def update(logger, wpt_dir, check_clean=True):
    localpaths = imp.load_source("localpaths",
                                 os.path.join(wpt_dir, "tests", "tools", "localpaths.py"))
    kwargs = {"config": os.path.join(wpt_dir, "wptrunner.ini"),
              "tests_root": None,
              "metadata_root": None}

    set_from_config(kwargs)
    config = kwargs["config"]
    test_paths = get_test_paths(config)

    if check_clean:
        old_manifests = {}
        for data in test_paths.itervalues():
            path = os.path.join(data["metadata_path"], "MANIFEST.json")
            with open(path) as f:
                old_manifests[path] = f.readlines()

    try:
        ManifestLoader(test_paths, force_manifest_update=True).load()

        rv = 0

        if check_clean:
            clean = diff_manifests(logger, old_manifests)
            if not clean:
                rv = 1
    finally:
        if check_clean:
            for path, data in old_manifests.iteritems():
                logger.info("Restoring manifest %s" % path)
                with open(path, "w") as f:
                    f.writelines(data)

    return rv

def diff_manifests(logger, old_manifests):
    logger.info("Diffing old and new manifests")
    import difflib

    clean = True
    for path, old in old_manifests.iteritems():
        with open(path) as f:
            new = f.readlines()

        if old != new:
            clean = False
            sm = difflib.SequenceMatcher(a=old, b=new)
            for group in sm.get_grouped_opcodes():
                logged = False
                message = []
                for op, old_0, old_1, new_0, new_1 in group:
                    if op != "equal" and not logged:
                        logged = True
                        logger.lint_error(path=path,
                                          message="Manifest changed",
                                          lineno=(old_0 + 1),
                                          source="\n".join(old[old_0:old_1]),
                                          linter="wpt-manifest")
                    if op == "equal":
                        message.extend(' ' + line for line in old[old_0:old_1])
                    if op in ('replace', 'delete'):
                        message.extend('-' + line for line in old[old_0:old_1])
                    if op in ('replace', 'insert'):
                        message.extend('+' + line for line in new[new_0:new_1])
                logger.info("".join(message))
    if clean:
        logger.info("No differences found")

    return clean

