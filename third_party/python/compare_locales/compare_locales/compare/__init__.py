# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Mozilla l10n compare locales tool'

from __future__ import absolute_import
from __future__ import print_function
import os
import shutil

from compare_locales import paths, mozpath

from .content import ContentComparer
from .observer import Observer, ObserverList
from .utils import Tree, AddRemove


__all__ = [
    'ContentComparer',
    'Observer', 'ObserverList',
    'AddRemove', 'Tree',
    'compareProjects',
]


def compareProjects(
            project_configs,
            locales,
            l10n_base_dir,
            stat_observer=None,
            merge_stage=None,
            clobber_merge=False,
            quiet=0,
        ):
    all_locales = set(locales)
    comparer = ContentComparer(quiet)
    observers = comparer.observers
    for project in project_configs:
        # disable filter if we're in validation mode
        if None in locales:
            filter = None
        else:
            filter = project.filter
        observers.append(
            Observer(
                quiet=quiet,
                filter=filter,
            ))
        if not locales:
            all_locales.update(project.all_locales)
    for locale in sorted(all_locales):
        files = paths.ProjectFiles(locale, project_configs,
                                   mergebase=merge_stage)
        if merge_stage is not None:
            if clobber_merge:
                mergematchers = set(_m.get('merge') for _m in files.matchers)
                mergematchers.discard(None)
                for matcher in mergematchers:
                    clobberdir = matcher.prefix
                    if os.path.exists(clobberdir):
                        shutil.rmtree(clobberdir)
                        print("clobbered " + clobberdir)
        for l10npath, refpath, mergepath, extra_tests in files:
            # module and file path are needed for legacy filter.py support
            module = None
            fpath = mozpath.relpath(l10npath, l10n_base_dir)
            for _m in files.matchers:
                if _m['l10n'].match(l10npath):
                    if _m['module']:
                        # legacy ini support, set module, and resolve
                        # local path against the matcher prefix,
                        # which includes the module
                        module = _m['module']
                        fpath = mozpath.relpath(l10npath, _m['l10n'].prefix)
                    break
            reffile = paths.File(refpath, fpath or refpath, module=module)
            if locale is None:
                # When validating the reference files, set locale
                # to a private subtag. This only shows in the output.
                locale = paths.REFERENCE_LOCALE
            l10n = paths.File(l10npath, fpath or l10npath,
                              module=module, locale=locale)
            if not os.path.exists(l10npath):
                comparer.add(reffile, l10n, mergepath)
                continue
            if not os.path.exists(refpath):
                comparer.remove(reffile, l10n, mergepath)
                continue
            comparer.compare(reffile, l10n, mergepath, extra_tests)
    return observers
