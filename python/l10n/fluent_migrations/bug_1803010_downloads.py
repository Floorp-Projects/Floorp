# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1803010 - Use .value consistently in downloads.ftl, part {index}."""

    for string in [
        "downloading-file-opens-in-hours-and-minutes",
        "downloading-file-opens-in-minutes",
        "downloading-file-opens-in-minutes-and-seconds",
        "downloading-file-opens-in-seconds",
        "downloading-file-opens-in-some-time",
    ]:
        ctx.add_transforms(
            "browser/browser/downloads.ftl",
            "browser/browser/downloads.ftl",
            transforms_from(
                """
{0}-2 =
    .value = {{COPY_PATTERN(from_path, "{0}")}}
                """.format(
                    string
                ),
                from_path="browser/browser/downloads.ftl",
            ),
        )
