# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from .transform import loader as transform_loader

logger = logging.getLogger(__name__)


DEFAULT_TRANSFORMS = [
    "taskgraph.transforms.job:transforms",
    "taskgraph.transforms.task:transforms",
]


def loader(kind, path, config, params, loaded_tasks):
    """
    This default loader builds on the `transform` loader by providing sensible
    default transforms that the majority of simple tasks will need.
    Specifically, `job` and `task` transforms will be appended to the end of the
    list of transforms in the kind being loaded.
    """
    transform_refs = config.setdefault("transforms", [])
    for t in DEFAULT_TRANSFORMS:
        if t in config.get("transforms", ()):
            raise KeyError(
                f"Transform {t} is already present in the loader's default transforms; it must not be defined in the kind"
            )
    transform_refs.extend(DEFAULT_TRANSFORMS)
    return transform_loader(kind, path, config, params, loaded_tasks)
