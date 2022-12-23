# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1347955 - Move the logging sub-page of about:networking to about:logging, part {index}"""
    ctx.add_transforms(
        "toolkit/toolkit/about/aboutLogging.ftl",
        "toolkit/toolkit/about/aboutLogging.ftl",
        transforms_from(
            """
about-logging-current-log-file =
    {COPY_PATTERN(from_path, "about-networking-current-log-file")}
about-logging-current-log-modules =
    {COPY_PATTERN(from_path, "about-networking-current-log-modules")}
about-logging-set-log-file =
    {COPY_PATTERN(from_path, "about-networking-set-log-file")}
about-logging-set-log-modules =
    {COPY_PATTERN(from_path, "about-networking-set-log-modules")}
about-logging-start-logging =
    {COPY_PATTERN(from_path, "about-networking-start-logging")}
about-logging-stop-logging =
    {COPY_PATTERN(from_path, "about-networking-stop-logging")}
about-logging-log-tutorial =
    {COPY_PATTERN(from_path, "about-networking-log-tutorial")}
    """,
            from_path="toolkit/toolkit/about/aboutNetworking.ftl",
        ),
    )
