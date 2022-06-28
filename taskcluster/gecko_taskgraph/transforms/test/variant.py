# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy

import jsone
from taskgraph.util.schema import Schema, validate_schema
from taskgraph.util.treeherder import join_symbol, split_symbol
from taskgraph.util.yaml import load_yaml
from voluptuous import (
    Any,
    Optional,
    Required,
)

import gecko_taskgraph
from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.templates import merge

transforms = TransformSequence()

TEST_VARIANTS = load_yaml(
    gecko_taskgraph.GECKO, "taskcluster", "ci", "test", "variants.yml"
)
"""List of available test variants defined."""


variant_description_schema = Schema(
    {
        str: {
            Required("description"): str,
            Required("suffix"): str,
            Required("component"): str,
            Optional("when"): {Any("$eval", "$if"): str},
            Optional("replace"): {str: object},
            Optional("merge"): {str: object},
        }
    }
)
"""variant description schema"""


@transforms.add
def split_variants(config, tasks):
    """Splits test definitions into multiple tasks based on the `variants` key.

    If `variants` are defined, the original task will be yielded along with a
    copy of the original task for each variant defined in the list. The copies
    will have the 'unittest_variant' attribute set.
    """
    validate_schema(variant_description_schema, TEST_VARIANTS, "In variants.yml:")

    def apply_variant(variant, task):
        task["description"] = variant["description"].format(**task)

        suffix = f"-{variant['suffix']}"
        group, symbol = split_symbol(task["treeherder-symbol"])
        if group != "?":
            group += suffix
        else:
            symbol += suffix
        task["treeherder-symbol"] = join_symbol(group, symbol)

        # This will be used to set the label and try-name in 'make_job_description'.
        task.setdefault("variant-suffix", "")
        task["variant-suffix"] += suffix

        # Replace and/or merge the configuration.
        task.update(variant.get("replace", {}))
        return merge(task, variant.get("merge", {}))

    for task in tasks:
        variants = task.pop("variants", [])

        if task.pop("run-without-variant"):
            yield copy.deepcopy(task)

        for name in variants:
            # Apply composite variants (joined by '+') in order.
            parts = name.split("+")
            taskv = copy.deepcopy(task)
            for part in parts:
                variant = TEST_VARIANTS[part]

                # If any variant in a composite fails this check we skip it.
                if "when" in variant:
                    context = {"task": task}
                    if not jsone.render(variant["when"], context):
                        break

                taskv = apply_variant(variant, taskv)
            else:
                taskv["attributes"]["unittest_variant"] = name
                yield taskv
