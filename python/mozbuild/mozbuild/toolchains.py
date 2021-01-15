# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os


def toolchain_task_definitions():
    from taskgraph.generator import load_tasks_for_kind

    # Don't import globally to allow this module being imported without
    # the taskgraph module being available (e.g. standalone js)
    params = {"level": os.environ.get("MOZ_SCM_LEVEL", "3")}
    root_dir = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "taskcluster", "ci"
    )
    toolchains = load_tasks_for_kind(params, "toolchain", root_dir=root_dir)
    aliased = {}
    for t in toolchains.values():
        alias = t.attributes.get("toolchain-alias")
        if alias:
            aliased["toolchain-{}".format(alias)] = t
    toolchains.update(aliased)

    return toolchains
