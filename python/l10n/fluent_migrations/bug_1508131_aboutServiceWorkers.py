# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate import REPLACE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1508131 - Migrate about:serviceworkers to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutServiceWorkers.ftl",
        "toolkit/toolkit/about/aboutServiceWorkers.ftl",
        transforms_from(
"""
about-service-workers-title = { COPY("toolkit/chrome/global/aboutServiceWorkers.dtd", "aboutServiceWorkers.title") }
about-service-workers-main-title = { COPY("toolkit/chrome/global/aboutServiceWorkers.dtd", "aboutServiceWorkers.maintitle") }
about-service-workers-warning-not-enabled = { COPY("toolkit/chrome/global/aboutServiceWorkers.dtd", "aboutServiceWorkers.warning_not_enabled") }
about-service-workers-warning-no-service-workers = { COPY("toolkit/chrome/global/aboutServiceWorkers.dtd", "aboutServiceWorkers.warning_no_serviceworkers") }

update-button = { COPY("toolkit/chrome/global/aboutServiceWorkers.properties", "update") }
unregister-button = { COPY("toolkit/chrome/global/aboutServiceWorkers.properties", "unregister") }
unregister-error = { COPY("toolkit/chrome/global/aboutServiceWorkers.properties", "unregisterError") }
waiting = { COPY("toolkit/chrome/global/aboutServiceWorkers.properties", "waiting") }
""")
)

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutServiceWorkers.ftl",
        "toolkit/toolkit/about/aboutServiceWorkers.ftl",
        [
        FTL.Message(
            id=FTL.Identifier("origin-title"),
            value=REPLACE(
                "toolkit/chrome/global/aboutServiceWorkers.properties",
                "title",
                {
                    "%S": VARIABLE_REFERENCE("originTitle"),
                },
            )
        ),

        FTL.Message(
            id=FTL.Identifier("app-title"),
            value=REPLACE(
                "toolkit/chrome/global/aboutServiceWorkers.properties",
                "b2gtitle",
                {
                    "%1$S": TERM_REFERENCE("-brand-short-name"),
                    "%2$S": VARIABLE_REFERENCE("appId"),
                    "%3$S": VARIABLE_REFERENCE("isInIsolatedElement"),
                },
            )
        ),

        FTL.Message(
            id=FTL.Identifier("scope"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "scope"
                ),
                FTL.TextElement('</strong> { $name }'),
            )
        ),

        FTL.Message(
            id=FTL.Identifier("script-spec"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "scriptSpec"
                ),
                FTL.TextElement('</strong> <a data-l10n-name="link">{ $url }</a>'),
            )
        ),

        FTL.Message(
            id=FTL.Identifier("current-worker-url"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "currentWorkerURL"
                ),
                FTL.TextElement('</strong> <a data-l10n-name="link">{ $url }</a>'),
            )
        ),

        FTL.Message(
            id=FTL.Identifier("active-cache-name"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "activeCacheName"
                ),
                FTL.TextElement('</strong> { $name }')
            )
        ),

        FTL.Message(
            id=FTL.Identifier("waiting-cache-name"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "waitingCacheName"
                ),
                FTL.TextElement('</strong> { $name }')
            )
        ),

        FTL.Message(
            id=FTL.Identifier("push-end-point-waiting"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "pushEndpoint"
                ),
                FTL.TextElement('</strong> { waiting }')
            )
        ),

        FTL.Message(
            id=FTL.Identifier("push-end-point-result"),
            value=CONCAT(
                FTL.TextElement('<strong>'),
                COPY(
                    "toolkit/chrome/global/aboutServiceWorkers.properties",
                    "pushEndpoint"
                ),
                FTL.TextElement('</strong> { $name }')
            )
        ),
    ]
)
