# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1845727 - move mozSupportLink l10n file to toolkit, part {index}."""
    browser_ftl = "browser/browser/components/mozSupportLink.ftl"
    toolkit_ftl = "toolkit/toolkit/global/mozSupportLink.ftl"
    ctx.add_transforms(
        toolkit_ftl,
        toolkit_ftl,
        transforms_from(
            """
moz-support-link-text = {COPY_PATTERN(from_path, "moz-support-link-text")}
""",
            from_path=browser_ftl,
        ),
    )
