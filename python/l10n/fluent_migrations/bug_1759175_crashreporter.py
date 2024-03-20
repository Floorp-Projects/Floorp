# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate.transforms import (
    CONCAT,
    REPLACE,
    REPLACE_IN_TEXT,
    LegacySource,
    Transform,
)


class SPLIT_AND_REPLACE(LegacySource):
    """Split sentence on '\n\n', return a specific segment, and perform replacements if needed"""

    def __init__(self, path, key, index, replacements=None, **kwargs):
        super(SPLIT_AND_REPLACE, self).__init__(path, key, **kwargs)
        self.index = index
        self.replacements = replacements

    def __call__(self, ctx):
        element = super(SPLIT_AND_REPLACE, self).__call__(ctx)
        segments = element.value.split(r"\n\n")
        element.value = segments[self.index]

        if self.replacements is None:
            return Transform.pattern_of(element)
        else:
            return REPLACE_IN_TEXT(element, self.replacements)(ctx)


def migrate(ctx):
    """Bug 1759175 - Migrate Crash Reporter to Fluent, part {index}."""

    target_file = "toolkit/crashreporter/crashreporter.ftl"
    source_file = "toolkit/crashreporter/crashreporter.ini"

    ctx.add_transforms(
        target_file,
        target_file,
        transforms_from(
            """
crashreporter-title = { COPY(from_path, "CrashReporterTitle") }
crashreporter-no-run-message = { COPY(from_path, "CrashReporterDefault") }
crashreporter-button-details = { COPY(from_path, "Details") }
crashreporter-view-report-title = { COPY(from_path, "ViewReportTitle") }
crashreporter-comment-prompt = { COPY(from_path, "CommentGrayText") }
crashreporter-report-info = { COPY(from_path, "ExtraReportInfo") }
crashreporter-submit-status = { COPY(from_path, "ReportPreSubmit2") }
crashreporter-submit-in-progress = { COPY(from_path, "ReportDuringSubmit2") }
crashreporter-submit-success = { COPY(from_path, "ReportSubmitSuccess") }
crashreporter-submit-failure = { COPY(from_path, "ReportSubmitFailed") }
crashreporter-resubmit-status = { COPY(from_path, "ReportResubmit") }
crashreporter-button-ok = { COPY(from_path, "Ok") }
crashreporter-button-close = { COPY(from_path, "Close") }
    """,
            from_path=source_file,
        ),
    )
    ctx.add_transforms(
        target_file,
        target_file,
        [
            FTL.Message(
                id=FTL.Identifier("crashreporter-crash-message"),
                value=SPLIT_AND_REPLACE(
                    source_file,
                    "CrashReporterDescriptionText2",
                    index=0,
                    replacements={
                        "%s": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("crashreporter-plea"),
                value=SPLIT_AND_REPLACE(
                    source_file,
                    "CrashReporterDescriptionText2",
                    index=1,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("crashreporter-error-details"),
                value=SPLIT_AND_REPLACE(
                    source_file,
                    "CrashReporterProductErrorText2",
                    index=2,
                    replacements={
                        "%s": VARIABLE_REFERENCE("details"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("crashreporter-button-quit"),
                value=REPLACE(
                    source_file,
                    "Quit2",
                    {
                        "%s": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("crashreporter-button-restart"),
                value=REPLACE(
                    source_file,
                    "Restart",
                    {
                        "%s": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("crashreporter-crash-identifier"),
                value=REPLACE(
                    source_file,
                    "CrashID",
                    {
                        "%s": VARIABLE_REFERENCE("id"),
                    },
                ),
            ),
        ],
    )
