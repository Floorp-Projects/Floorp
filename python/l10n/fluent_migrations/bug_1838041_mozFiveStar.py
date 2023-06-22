# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1838041 - Create MozFiveStar reusable component , part {index}."""
    ctx.add_transforms(
        "browser/browser/components/mozFiveStar.ftl",
        "browser/browser/components/mozFiveStar.ftl",
        transforms_from(
            """
moz-five-star-rating =
  .title = {COPY_PATTERN(from_path, "five-star-rating.title")}
    """,
            from_path="toolkit/toolkit/about/aboutAddons.ftl",
        ),
    )
