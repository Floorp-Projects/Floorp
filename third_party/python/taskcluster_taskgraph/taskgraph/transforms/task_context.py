from textwrap import dedent

from voluptuous import ALLOW_EXTRA, Any, Optional, Required

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema
from taskgraph.util.templates import deep_get, substitute
from taskgraph.util.yaml import load_yaml

SCHEMA = Schema(
    {
        Required(
            "task-context",
            description=dedent(
                """
            `task-context` can be used to substitute values into any field in a
            task with data that is not known until `taskgraph` runs.

            This data can be provided via `from-parameters` or `from-file`,
            which can pull in values from parameters and a defined yml file
            respectively.

            Data may also be provided directly in the `from-object` section of
            `task-context`. This can be useful in `kinds` that define most of
            their contents in `task-defaults`, but have some values that may
            differ for various concrete `tasks` in the `kind`.

            If the same key is found in multiple places the order of precedence
            is as follows:
              - Parameters
              - `from-object` keys
              - File

            That is to say: parameters will always override anything else.

            """.lstrip(),
            ),
        ): {
            Optional(
                "from-parameters",
                description=dedent(
                    """
                Retrieve task context values from parameters. A single
                parameter may be provided or a list of parameters in
                priority order. The latter can be useful in implementing a
                "default" value if some other parameter is not provided.
                """.lstrip()
                ),
            ): {str: Any([str], str)},
            Optional(
                "from-file",
                description=dedent(
                    """
                Retrieve task context values from a yaml file. The provided
                file should usually only contain top level keys and values
                (eg: nested objects will not be interpolated - they will be
                substituted as text representations of the object).
                """.lstrip()
                ),
            ): str,
            Optional(
                "from-object",
                description="Key/value pairs to be used as task context",
            ): object,
            Required(
                "substitution-fields",
                description=dedent(
                    """
                A list of fields in the task to substitute the provided values
                into.
                """.lstrip()
                ),
            ): [str],
        },
    },
    extra=ALLOW_EXTRA,
)

transforms = TransformSequence()
transforms.add_validate(SCHEMA)


@transforms.add
def render_task(config, jobs):
    for job in jobs:
        sub_config = job.pop("task-context")
        params_context = {}
        for var, path in sub_config.pop("from-parameters", {}).items():
            if isinstance(path, str):
                params_context[var] = deep_get(config.params, path)
            else:
                for choice in path:
                    value = deep_get(config.params, choice)
                    if value is not None:
                        params_context[var] = value
                        break

        file_context = {}
        from_file = sub_config.pop("from-file", None)
        if from_file:
            file_context = load_yaml(from_file)

        fields = sub_config.pop("substitution-fields")

        subs = {}
        subs.update(file_context)
        # We've popped away the configuration; everything left in `sub_config` is
        # substitution key/value pairs.
        subs.update(sub_config.pop("from-object", {}))
        subs.update(params_context)

        # Now that we have our combined context, we can substitute.
        for field in fields:
            container, subfield = job, field
            while "." in subfield:
                f, subfield = subfield.split(".", 1)
                container = container[f]

            container[subfield] = substitute(container[subfield], **subs)

        yield job
