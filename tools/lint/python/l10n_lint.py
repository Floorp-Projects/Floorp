# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from datetime import datetime, timedelta
from subprocess import check_call

from compare_locales import parser
from compare_locales.lint.linter import L10nLinter
from compare_locales.lint.util import l10n_base_reference_and_tests
from compare_locales.paths import ProjectFiles, TOMLParser
from mach import util as mach_util
from mozfile import which
from mozlint import pathutils, result
from mozpack import path as mozpath
from mozversioncontrol import MissingVCSTool

L10N_SOURCE_NAME = "l10n-source"
L10N_SOURCE_REPO = "https://github.com/mozilla-l10n/firefox-l10n-source.git"

PULL_AFTER = timedelta(days=2)


# Wrapper to call lint_strings with mozilla-central configuration
# comm-central defines its own wrapper since comm-central strings are
# in separate repositories
def lint(paths, lintconfig, **lintargs):
    return lint_strings(L10N_SOURCE_NAME, paths, lintconfig, **lintargs)


def lint_strings(name, paths, lintconfig, **lintargs):
    l10n_base = mach_util.get_state_dir()
    root = lintargs["root"]
    exclude = lintconfig.get("exclude")
    extensions = lintconfig.get("extensions")

    # Load l10n.toml configs
    l10nconfigs = load_configs(lintconfig["l10n_configs"], root, l10n_base, name)

    # If l10n.yml is included in the provided paths, validate it against the
    # TOML files, then remove it to avoid parsing it as a localizable resource.
    if lintconfig["path"] in paths:
        results = validate_linter_includes(lintconfig, l10nconfigs, lintargs)
        paths.remove(lintconfig["path"])
        lintconfig["include"].remove(mozpath.relpath(lintconfig["path"], root))
    else:
        results = []

    all_files = []
    for p in paths:
        fp = pathutils.FilterPath(p)
        if fp.isdir:
            for _, fileobj in fp.finder:
                all_files.append(fileobj.path)
        if fp.isfile:
            all_files.append(p)
    # Filter out files explicitly excluded in the l10n.yml configuration.
    # `browser/locales/en-US/firefox-l10n.js` is a good example.
    all_files, _ = pathutils.filterpaths(
        lintargs["root"],
        all_files,
        lintconfig["include"],
        exclude=exclude,
        extensions=extensions,
    )
    # Filter again, our directories might have picked up files that should be
    # excluded in l10n.yml
    skips = {p for p in all_files if not parser.hasParser(p)}
    results.extend(
        result.from_config(
            lintconfig,
            level="warning",
            path=path,
            message="file format not supported in compare-locales",
        )
        for path in skips
    )
    all_files = [p for p in all_files if p not in skips]
    files = ProjectFiles(name, l10nconfigs)

    get_reference_and_tests = l10n_base_reference_and_tests(files)

    linter = MozL10nLinter(lintconfig)
    results += linter.lint(all_files, get_reference_and_tests)
    return results


# Similar to the lint/lint_strings wrapper setup, for comm-central support.
def source_repo_setup(**lint_args):
    gs = mozpath.join(mach_util.get_state_dir(), L10N_SOURCE_NAME)
    marker = mozpath.join(gs, ".git", "l10n_pull_marker")
    try:
        last_pull = datetime.fromtimestamp(os.stat(marker).st_mtime)
        skip_clone = datetime.now() < last_pull + PULL_AFTER
    except OSError:
        skip_clone = False
    if skip_clone:
        return
    git = which("git")
    if not git:
        if os.environ.get("MOZ_AUTOMATION"):
            raise MissingVCSTool("Unable to obtain git path.")
        print("warning: l10n linter requires Git but was unable to find 'git'")
        return 1
    if os.path.exists(gs):
        check_call([git, "pull", L10N_SOURCE_REPO], cwd=gs)
    else:
        check_call([git, "clone", L10N_SOURCE_REPO, gs])
    with open(marker, "w") as fh:
        fh.flush()


def load_configs(l10n_configs, root, l10n_base, locale):
    """Load l10n configuration files specified in the linter configuration."""
    configs = []
    env = {"l10n_base": l10n_base}
    for toml in l10n_configs:
        cfg = TOMLParser().parse(
            mozpath.join(root, toml), env=env, ignore_missing_includes=True
        )
        cfg.set_locales([locale], deep=True)
        configs.append(cfg)
    return configs


def validate_linter_includes(lintconfig, l10nconfigs, lintargs):
    """Check l10n.yml config against l10n.toml configs."""
    reference_paths = set(
        mozpath.relpath(p["reference"].prefix, lintargs["root"])
        for project in l10nconfigs
        for config in project.configs
        for p in config.paths
    )
    # Just check for directories
    reference_dirs = sorted(p for p in reference_paths if os.path.isdir(p))
    missing_in_yml = [
        refd for refd in reference_dirs if refd not in lintconfig["include"]
    ]
    # These might be subdirectories in the config, though
    missing_in_yml = [
        d
        for d in missing_in_yml
        if not any(d.startswith(parent + "/") for parent in lintconfig["include"])
    ]
    if missing_in_yml:
        dirs = ", ".join(missing_in_yml)
        return [
            result.from_config(
                lintconfig,
                path=lintconfig["path"],
                message="l10n.yml out of sync with l10n.toml, add: " + dirs,
            )
        ]
    return []


class MozL10nLinter(L10nLinter):
    """Subclass linter to generate the right result type."""

    def __init__(self, lintconfig):
        super(MozL10nLinter, self).__init__()
        self.lintconfig = lintconfig

    def lint(self, files, get_reference_and_tests):
        return [
            result.from_config(self.lintconfig, **result_data)
            for result_data in super(MozL10nLinter, self).lint(
                files, get_reference_and_tests
            )
        ]
