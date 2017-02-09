# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from . import transform


class BeetmoverChecksumsTask(transform.TransformTask):
    """
    A task implementing a beetmover job specific for checksums.These depend on
    the checksums signing jobs and transfer the checksums files to S3 after
    it's being generated and signed.
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        if config.get('kind-dependencies', []) != ["checksums-signing"]:
            raise Exception("Beetmover checksums tasks depend on checksums signing tasks")
        for task in loaded_tasks:
            if not task.attributes.get('nightly'):
                continue
            if task.kind not in config.get('kind-dependencies'):
                continue
            beetmover_checksums_task = {'dependent-task': task}

            yield beetmover_checksums_task
