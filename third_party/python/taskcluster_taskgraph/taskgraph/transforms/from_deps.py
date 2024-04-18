# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Transforms used to create tasks based on the kind dependencies, filtering on
common attributes like the ``build-type``.

These transforms are useful when follow-up tasks are needed for some
indeterminate subset of existing tasks. For example, running a signing task
after each build task, whatever builds may exist.
"""
from copy import deepcopy
from textwrap import dedent

from voluptuous import Any, Extra, Optional, Required

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.job import fetches_schema
from taskgraph.util.attributes import attrmatch
from taskgraph.util.dependencies import GROUP_BY_MAP, get_dependencies
from taskgraph.util.schema import Schema, validate_schema

FROM_DEPS_SCHEMA = Schema(
    {
        Required("from-deps"): {
            Optional(
                "kinds",
                description=dedent(
                    """
                Limit dependencies to specified kinds (defaults to all kinds in
                `kind-dependencies`).

                The first kind in the list is the "primary" kind. The
                dependency of this kind will be used to derive the label
                and copy attributes (if `copy-attributes` is True).
                """.lstrip()
                ),
            ): list,
            Optional(
                "set-name",
                description=dedent(
                    """
                When True, `from_deps` will derive a name for the generated
                tasks from the name of the primary dependency. Defaults to
                True.
                """.lstrip()
                ),
            ): bool,
            Optional(
                "with-attributes",
                description=dedent(
                    """
                Limit dependencies to tasks whose attributes match
                using :func:`~taskgraph.util.attributes.attrmatch`.
                """.lstrip()
                ),
            ): {str: Any(list, str)},
            Optional(
                "group-by",
                description=dedent(
                    """
                Group cross-kind dependencies using the given group-by
                function. One task will be created for each group. If not
                specified, the 'single' function will be used which creates
                a new task for each individual dependency.
                """.lstrip()
                ),
            ): Any(
                None,
                *GROUP_BY_MAP,
                {Any(*GROUP_BY_MAP): object},
            ),
            Optional(
                "copy-attributes",
                description=dedent(
                    """
                If True, copy attributes from the dependency matching the
                first kind in the `kinds` list (whether specified explicitly
                or taken from `kind-dependencies`).
                """.lstrip()
                ),
            ): bool,
            Optional(
                "unique-kinds",
                description=dedent(
                    """
                If true (the default), there must be only a single unique task
                for each kind in a dependency group. Setting this to false
                disables that requirement.
                """.lstrip()
                ),
            ): bool,
            Optional(
                "fetches",
                description=dedent(
                    """
                If present, a `fetches` entry will be added for each task
                dependency. Attributes of the upstream task may be used as
                substitution values in the `artifact` or `dest` values of the
                `fetches` entry.
                """.lstrip()
                ),
            ): {str: [fetches_schema]},
        },
        Extra: object,
    },
)
"""Schema for from_deps transforms."""

transforms = TransformSequence()
transforms.add_validate(FROM_DEPS_SCHEMA)


@transforms.add
def from_deps(config, tasks):
    for task in tasks:
        # Setup and error handling.
        from_deps = task.pop("from-deps")
        kind_deps = config.config.get("kind-dependencies", [])
        kinds = from_deps.get("kinds", kind_deps)

        invalid = set(kinds) - set(kind_deps)
        if invalid:
            invalid = "\n".join(sorted(invalid))
            raise Exception(
                dedent(
                    f"""
                    The `from-deps.kinds` key contains the following kinds
                    that are not defined in `kind-dependencies`:
                    {invalid}
                """.lstrip()
                )
            )

        if not kinds:
            raise Exception(
                dedent(
                    """
                The `from_deps` transforms require at least one kind defined
                in `kind-dependencies`!
                """.lstrip()
                )
            )

        # Resolve desired dependencies.
        with_attributes = from_deps.get("with-attributes")
        deps = [
            task
            for task in config.kind_dependencies_tasks.values()
            if task.kind in kinds
            if not with_attributes or attrmatch(task.attributes, **with_attributes)
        ]

        # Resolve groups.
        group_by = from_deps.get("group-by", "single")
        groups = set()

        if isinstance(group_by, dict):
            assert len(group_by) == 1
            group_by, arg = group_by.popitem()
            func = GROUP_BY_MAP[group_by]
            if func.schema:
                validate_schema(
                    func.schema, arg, f"Invalid group-by {group_by} argument"
                )
            groups = func(config, deps, arg)
        else:
            func = GROUP_BY_MAP[group_by]
            groups = func(config, deps)

        # Split the task, one per group.
        set_name = from_deps.get("set-name", True)
        copy_attributes = from_deps.get("copy-attributes", False)
        unique_kinds = from_deps.get("unique-kinds", True)
        fetches = from_deps.get("fetches", [])
        for group in groups:
            # Verify there is only one task per kind in each group.
            group_kinds = {t.kind for t in group}
            if unique_kinds and len(group_kinds) < len(group):
                raise Exception(
                    "The from_deps transforms only allow a single task per kind in a group!"
                )

            new_task = deepcopy(task)
            new_task.setdefault("dependencies", {})
            new_task["dependencies"].update(
                {dep.kind if unique_kinds else dep.label: dep.label for dep in group}
            )

            # Set name and copy attributes from the primary kind.
            for kind in kinds:
                if kind in group_kinds:
                    primary_kind = kind
                    break
            else:
                raise Exception("Could not detect primary kind!")

            new_task.setdefault("attributes", {})[
                "primary-kind-dependency"
            ] = primary_kind

            primary_dep = [dep for dep in group if dep.kind == primary_kind][0]

            if set_name:
                if primary_dep.label.startswith(primary_kind):
                    new_task["name"] = primary_dep.label[len(primary_kind) + 1 :]
                else:
                    new_task["name"] = primary_dep.label

            if copy_attributes:
                attrs = new_task.setdefault("attributes", {})
                new_task["attributes"] = primary_dep.attributes.copy()
                new_task["attributes"].update(attrs)

            if fetches:
                task_fetches = new_task.setdefault("fetches", {})

                for dep_task in get_dependencies(config, new_task):
                    # Nothing to do if this kind has no fetches listed
                    if dep_task.kind not in fetches:
                        continue

                    fetches_from_dep = []
                    for kind, kind_fetches in fetches.items():
                        if kind != dep_task.kind:
                            continue

                        for fetch in kind_fetches:
                            entry = fetch.copy()
                            entry["artifact"] = entry["artifact"].format(
                                **dep_task.attributes
                            )
                            if "dest" in entry:
                                entry["dest"] = entry["dest"].format(
                                    **dep_task.attributes
                                )
                            fetches_from_dep.append(entry)

                    task_fetches[dep_task.label] = fetches_from_dep

            yield new_task
