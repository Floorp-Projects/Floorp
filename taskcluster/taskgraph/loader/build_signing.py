# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import loader as base_loader

# XXX: This logic should rely in kind.yml. This hasn't been done in the original
# patch because it required some heavy changes in single_dep.
NON_NIGHTLY_LABELS_WHICH_SHOULD_SIGN_BUILDS = (
    'build-win32/debug', 'build-win32/opt', 'build-win32/pgo',
    'build-win64/debug', 'build-win64/opt', 'build-win64/pgo',
    'build-win32-devedition/opt', 'build-win64-devedition/opt',
    'build-win64-ccov/debug',
    'build-linux/opt', 'build-linux64/opt', 'build-macosx64/opt',
    'build-android-api-16/opt'
    'release-source-linux64-source/opt',
    'release-source-linux64-fennec-source/opt',
    'release-source-linux64-devedition-source/opt',
    'release-eme-free-repack-macosx64-nightly',
    'release-partner-repack-macosx64-nightly',
)


def loader(kind, path, config, params, loaded_tasks):
    jobs = base_loader(kind, path, config, params, loaded_tasks)

    for job in jobs:
        dependent_task = job['dependent-task']
        if dependent_task.attributes.get('nightly') or \
                dependent_task.label in NON_NIGHTLY_LABELS_WHICH_SHOULD_SIGN_BUILDS:
            yield job
