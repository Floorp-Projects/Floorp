import pydoc


def translate_options(ctx, param, value):
    text = """Target language options for Translate function

These are backend specific and optional, provide as key:value

Rust: no options.

Swift:
- `namespace`: The namespace to generate metrics in
- `glean_namespace`: The namespace to import Glean from
- `allow_reserved`: When True, this is a Glean-internal build
- `with_buildinfo`: If "true" the `GleanBuildInfo` is generated.
        Otherwise generation of that file is skipped.
        Defaults to "true".
- `build_date`: If set to `0` a static unix epoch time will be used.
        If set to a ISO8601 datetime string (e.g. `2022-01-03T17:30:00`)
        it will use that date.
        Other values will throw an error.
        If not set it will use the current date & time.

Kotlin:
- `namespace`: The package namespace to declare at the top of the
        generated files. Defaults to `GleanMetrics`.
- `glean_namespace`: The package namespace of the glean library itself.
        This is where glean objects will be imported from in the generated
        code.

JavaScript:
- `platform`: Which platform are we building for. Options are `webext` and `qt`.
        Default is `webext`.
- `version`: The version of the Glean.js Qt library being used.
        This option is mandatory when targeting Qt. Note that the version
        string must only contain the major and minor version i.e. 0.14.
- `with_buildinfo`: If "true" a `gleanBuildInfo.(js|ts)` file is generated.
        Otherwise generation of that file is skipped. Defaults to "false".
- `build_date`: If set to `0` a static unix epoch time will be used.
        If set to a ISO8601 datetime string (e.g. `2022-01-03T17:30:00`)
        it will use that date.
        Other values will throw an error.
        If not set it will use the current date & time.
- `fail_rates`: When `true` it fails when encountering rate metrics.
                When `false` it will warn and skip rate metrics.
                Defaults to "true".

Markdown:
- `project_title`: The project's title.

(press q to exit)"""

    if value:
        if value[0].lower() == "help":
            pydoc.pager(text)
            ctx.exit()
    return value
