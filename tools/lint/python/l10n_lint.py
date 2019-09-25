# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

from datetime import datetime, timedelta
import os

from mozboot import util as mb_util
from mozlint import result, pathutils
from mozpack import path as mozpath
import mozversioncontrol.repoupdate

from compare_locales.lint.linter import L10nLinter
from compare_locales.lint.util import l10n_base_reference_and_tests
from compare_locales import parser
from compare_locales.paths import TOMLParser, ProjectFiles


LOCALE = 'gecko-strings'


PULL_AFTER = timedelta(days=2)


def lint(paths, lintconfig, **lintargs):
    l10n_base = mb_util.get_state_dir()
    root = lintargs['root']
    exclude = lintconfig.get('exclude')
    extensions = lintconfig.get('extensions')

    # Load l10n.toml configs
    l10nconfigs = load_configs(lintconfig, root, l10n_base)

    # Check include paths in l10n.yml if it's in our given paths
    # Only the l10n.yml will show up here, but if the l10n.toml files
    # change, we also get the l10n.yml as the toml files are listed as
    # support files.
    if lintconfig['path'] in paths:
        results = validate_linter_includes(lintconfig, l10nconfigs, lintargs)
        paths.remove(lintconfig['path'])
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
    # Filter again, our directories might have picked up files the
    # explicitly excluded in the l10n.yml configuration.
    # `browser/locales/en-US/firefox-l10n.js` is a good example.
    all_files, _ = pathutils.filterpaths(
        lintargs['root'], all_files, lintconfig['include'],
        exclude=exclude, extensions=extensions
    )
    # These should be excluded in l10n.yml
    skips = {p for p in all_files if not parser.hasParser(p)}
    results.extend(
        result.from_config(
            lintconfig,
            level='warning',
            path=path,
            message="file format not supported in compare-locales"
            )
        for path in skips
    )
    all_files = [p for p in all_files if p not in skips]
    files = ProjectFiles(LOCALE, l10nconfigs)

    get_reference_and_tests = l10n_base_reference_and_tests(files)

    linter = MozL10nLinter(lintconfig)
    results += linter.lint(all_files, get_reference_and_tests)
    return results


def gecko_strings_setup(**lint_args):
    gs = mozpath.join(mb_util.get_state_dir(), LOCALE)
    marker = mozpath.join(gs, '.hg', 'l10n_pull_marker')
    try:
        last_pull = datetime.fromtimestamp(os.stat(marker).st_mtime)
        skip_clone = datetime.now() < last_pull + PULL_AFTER
    except OSError:
        skip_clone = False
    if skip_clone:
        return
    hg = mozversioncontrol.get_tool_path('hg')
    mozversioncontrol.repoupdate.update_mercurial_repo(
        hg,
        'https://hg.mozilla.org/l10n/gecko-strings',
        gs
    )
    with open(marker, 'w') as fh:
        fh.flush()


def load_configs(lintconfig, root, l10n_base):
    '''Load l10n configuration files specified in the linter configuration.'''
    configs = []
    env = {
        'l10n_base': l10n_base
    }
    for toml in lintconfig['l10n_configs']:
        cfg = TOMLParser().parse(
            mozpath.join(root, toml),
            env=env,
            ignore_missing_includes=True
        )
        cfg.set_locales([LOCALE], deep=True)
        configs.append(cfg)
    return configs


def validate_linter_includes(lintconfig, l10nconfigs, lintargs):
    '''Check l10n.yml config against l10n.toml configs.'''
    reference_paths = set(
        mozpath.relpath(p['reference'].prefix, lintargs['root'])
        for project in l10nconfigs
        for config in project.configs
        for p in config.paths
    )
    # Just check for directories
    reference_dirs = sorted(p for p in reference_paths if os.path.isdir(p))
    missing_in_yml = [
        refd for refd in reference_dirs if refd not in lintconfig['include']
    ]
    # These might be subdirectories in the config, though
    missing_in_yml = [
        d for d in missing_in_yml
        if not any(d.startswith(parent + '/') for parent in lintconfig['include'])
    ]
    if missing_in_yml:
        dirs = ', '.join(missing_in_yml)
        return [result.from_config(
            lintconfig, path=lintconfig['path'],
            message='l10n.yml out of sync with l10n.toml, add: ' + dirs
        )]
    return []


class MozL10nLinter(L10nLinter):
    '''Subclass linter to generate the right result type.'''
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
