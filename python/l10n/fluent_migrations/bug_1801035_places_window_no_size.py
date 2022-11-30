# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1801035 - Stop localizing places window style, part {index}."""

    ctx.add_transforms(
        "browser/browser/places.ftl",
        "browser/browser/places.ftl",
        transforms_from(
            """
places-library3 =
    .title = {{COPY_PATTERN(from_path, "places-library2.title")}}
            """,
            from_path="browser/browser/places.ftl",
        ),
    )
