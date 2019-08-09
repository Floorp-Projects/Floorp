# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def use_toolchains(config, jobs):
    """Transform toolchains definitions into the corresponding fetch
    definitions.
    """
    for job in jobs:
        toolchains = job.pop('toolchains', None)
        if toolchains:
            job.setdefault('fetches', {}) \
               .setdefault('toolchain', []) \
               .extend(toolchains)
        yield job
