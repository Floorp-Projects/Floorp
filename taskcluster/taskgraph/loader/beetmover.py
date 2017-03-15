# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals


def loader(kind, path, config, params, loaded_tasks):
    """
    Generate inputs implementing beetmover jobs.  These depend on nightly build
    and signing jobs and transfer the artifacts to S3 after build and signing
    are completed.
    """
    if config.get('kind-dependencies', []) != ["build-signing"] and \
       config.get('kind-dependencies', []) != ["nightly-l10n-signing"]:
        raise Exception("Beetmover kinds must depend on builds or signing builds")
    for task in loaded_tasks:
        if not task.attributes.get('nightly'):
            continue
        if task.kind not in config.get('kind-dependencies'):
            continue
        beetmover_task = {'dependent-task': task}

        yield beetmover_task
