# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY_PATTERN, TransformPattern


class STRIP_SPAN(TransformPattern):
    # Used to remove `<span data-l10n-name="addon-name"></span>` from a string
    def visit_TextElement(self, node):
        span_start = node.value.find('<span data-l10n-name="addon-name">')
        span_end = node.value.find("</span>")
        if span_start != -1 and span_end == -1:
            node.value = node.value[:span_start]
        elif span_start == -1 and span_end != -1:
            node.value = node.value[span_end + 7 :]

        return node


def migrate(ctx):
    """Bug 1845120 - Use moz-message-bar for abuse report messages in about:addons, part {index}."""
    abuseReports_ftl = "toolkit/toolkit/about/abuseReports.ftl"
    ctx.add_transforms(
        abuseReports_ftl,
        abuseReports_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-aborted2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-aborted",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-submitting2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-submitting",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-submitted2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-submitted",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-submitted-noremove2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=COPY_PATTERN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-submitted-noremove",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-removed-extension2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-removed-extension",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-removed-sitepermission2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-removed-sitepermission",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-removed-theme2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-removed-theme",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-error2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-error",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("abuse-report-messagebar-error-recent-submit2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            abuseReports_ftl,
                            "abuse-report-messagebar-error-recent-submit",
                        ),
                    ),
                ],
            ),
        ],
    )
