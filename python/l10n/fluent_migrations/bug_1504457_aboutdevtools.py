from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT

def migrate(ctx):
    """Bug 1504457 -  Migrate subscribe.js strings of about:devtools to Fluent, part {index}"""

    ctx.add_transforms(
        "devtools/startup/aboutDevTools.ftl",
        "devtools/startup/aboutDevTools.ftl",
        transforms_from(
"""
newsletter-error-unknown = { COPY("devtools/startup/aboutdevtools.properties", "newsletter.error.unknown")}
newsletter-error-timeout = { COPY("devtools/startup/aboutdevtools.properties", "newsletter.error.timeout")}
"""
        )
    )

    ctx.add_transforms(
        "devtools/startup/aboutDevTools.ftl",
        "devtools/startup/aboutDevTools.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("newsletter-error-common"),
                value=REPLACE(
                    "devtools/startup/aboutdevtools.properties",
                    "newsletter.error.common",
                    {
                        "%S": VARIABLE_REFERENCE("errorDescription")
                    }
                )
            )
        ]
    )
