# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1838632 - Remove various non-standard attributes from fluent files, part {index}."""

    connection_ftl = "browser/browser/preferences/connection.ftl"
    ctx.add_transforms(
        connection_ftl,
        connection_ftl,
        transforms_from(
            """
connection-proxy-autologin-checkbox =
    .label = {COPY_PATTERN(from_path, "connection-proxy-autologin.label")}
    .accesskey = {COPY_PATTERN(from_path, "connection-proxy-autologin.accesskey")}
    .tooltiptext = {COPY_PATTERN(from_path, "connection-proxy-autologin.tooltip")}
    """,
            from_path=connection_ftl,
        ),
    )
