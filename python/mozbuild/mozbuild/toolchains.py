# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import six


def toolchain_task_definitions():
    import gecko_taskgraph  # noqa: triggers override of the `graph_config_schema`
    from taskgraph.generator import load_tasks_for_kind

    # Don't import globally to allow this module being imported without
    # the taskgraph module being available (e.g. standalone js)
    params = {"level": os.environ.get("MOZ_SCM_LEVEL", "3")}
    root_dir = os.path.join(os.path.dirname(__file__), "..", "..", "..", "taskcluster")
    toolchains = load_tasks_for_kind(params, "toolchain", root_dir=root_dir)
    aliased = {}
    for t in toolchains.values():
        aliases = t.attributes.get("toolchain-alias")
        if not aliases:
            aliases = []
        if isinstance(aliases, six.text_type):
            aliases = [aliases]
        for alias in aliases:
            aliased["toolchain-{}".format(alias)] = t
    toolchains.update(aliased)

    return toolchains
