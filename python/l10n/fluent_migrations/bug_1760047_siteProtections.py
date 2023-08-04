# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1760047 - Migrate browser-siteProtections.js to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    target = "browser/browser/siteProtections.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("content-blocking-trackers-view-empty"),
                value=COPY(source, "contentBlocking.trackersView.empty.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-blocking-trackers-label"),
                value=COPY(source, "contentBlocking.cookies.blockingTrackers3.label"),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "content-blocking-cookies-blocking-third-party-label"
                ),
                value=COPY(source, "contentBlocking.cookies.blocking3rdParty2.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-blocking-unvisited-label"),
                value=COPY(source, "contentBlocking.cookies.blockingUnvisited2.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-blocking-all-label"),
                value=COPY(source, "contentBlocking.cookies.blockingAll2.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-first-party-label"),
                value=COPY(source, "contentBlocking.cookiesView.firstParty.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-trackers-label"),
                value=COPY(source, "contentBlocking.cookiesView.trackers2.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-third-party-label"),
                value=COPY(source, "contentBlocking.cookiesView.thirdParty.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-allowed-label"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY(source, "contentBlocking.cookiesView.allowed.label"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-blocked-label"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY(source, "contentBlocking.cookiesView.blocked.label"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("content-blocking-cookies-view-remove-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            source,
                            "contentBlocking.cookiesView.removeButton.tooltip",
                            {"%1$S": VARIABLE_REFERENCE("domain")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tracking-protection-icon-active"),
                value=COPY(source, "trackingProtection.icon.activeTooltip2"),
            ),
            FTL.Message(
                id=FTL.Identifier("tracking-protection-icon-active-container"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=FTL.Pattern(
                            [
                                FTL.Placeable(
                                    MESSAGE_REFERENCE("tracking-protection-icon-active")
                                )
                            ]
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tracking-protection-icon-disabled"),
                value=COPY(source, "trackingProtection.icon.disabledTooltip2"),
            ),
            FTL.Message(
                id=FTL.Identifier("tracking-protection-icon-disabled-container"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=FTL.Pattern(
                            [
                                FTL.Placeable(
                                    MESSAGE_REFERENCE(
                                        "tracking-protection-icon-disabled"
                                    )
                                )
                            ]
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tracking-protection-icon-no-trackers-detected"),
                value=REPLACE(
                    source,
                    "trackingProtection.icon.noTrackersDetectedTooltip",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "tracking-protection-icon-no-trackers-detected-container"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=FTL.Pattern(
                            [
                                FTL.Placeable(
                                    MESSAGE_REFERENCE(
                                        "tracking-protection-icon-no-trackers-detected"
                                    )
                                )
                            ]
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-header"),
                value=REPLACE(
                    source, "protections.header", {"%1$S": VARIABLE_REFERENCE("host")}
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("protections-disable"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=REPLACE(
                            source,
                            "protections.disableAriaLabel",
                            {"%1$S": VARIABLE_REFERENCE("host")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-enable"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=REPLACE(
                            source,
                            "protections.enableAriaLabel",
                            {"%1$S": VARIABLE_REFERENCE("host")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-fingerprinters"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "protections.blocking.fingerprinters.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-cryptominers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "protections.blocking.cryptominers.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-cookies-trackers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.blocking.cookies.trackers.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-cookies-third-party"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.blocking.cookies.3rdParty.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-cookies-all"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "protections.blocking.cookies.all.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-cookies-unvisited"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.blocking.cookies.unvisited.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-tracking-content"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.blocking.trackingContent.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-blocking-social-media-trackers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.blocking.socialMediaTrackers.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-fingerprinters"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.notBlocking.fingerprinters.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-cryptominers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.notBlocking.cryptominers.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-cookies-third-party"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.notBlocking.cookies.3rdParty.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-cookies-all"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "protections.notBlocking.cookies.all.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "protections-not-blocking-cross-site-tracking-cookies"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source,
                            "protections.notBlocking.crossSiteTrackingCookies.title",
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-tracking-content"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.notBlocking.trackingContent.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-not-blocking-social-media-trackers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            source, "protections.notBlocking.socialMediaTrackers.title"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-footer-blocked-tracker-counter"),
                value=PLURALS(
                    source,
                    "protections.footer.blockedTrackerCounter.description",
                    VARIABLE_REFERENCE("trackerCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("trackerCount")},
                    ),
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            source,
                            "protections.footer.blockedTrackerCounter.tooltip",
                            {
                                "%1$S": FTL.FunctionReference(
                                    FTL.Identifier("DATETIME"),
                                    FTL.CallArguments(
                                        positional=[VARIABLE_REFERENCE("date")],
                                        named=[
                                            FTL.NamedArgument(
                                                FTL.Identifier("year"),
                                                FTL.StringLiteral("numeric"),
                                            ),
                                            FTL.NamedArgument(
                                                FTL.Identifier("month"),
                                                FTL.StringLiteral("long"),
                                            ),
                                            FTL.NamedArgument(
                                                FTL.Identifier("day"),
                                                FTL.StringLiteral("numeric"),
                                            ),
                                        ],
                                    ),
                                ),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("protections-milestone"),
                value=PLURALS(
                    source,
                    "protections.milestone.description",
                    VARIABLE_REFERENCE("trackerCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("trackerCount"),
                            "#3": FTL.FunctionReference(
                                FTL.Identifier("DATETIME"),
                                FTL.CallArguments(
                                    positional=[VARIABLE_REFERENCE("date")],
                                    named=[
                                        FTL.NamedArgument(
                                            FTL.Identifier("year"),
                                            FTL.StringLiteral("numeric"),
                                        ),
                                        FTL.NamedArgument(
                                            FTL.Identifier("month"),
                                            FTL.StringLiteral("long"),
                                        ),
                                    ],
                                ),
                            ),
                        },
                    ),
                ),
            ),
        ],
    )
