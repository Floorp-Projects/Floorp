# -*- coding: utf-8 -*-

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1523741 - Migrate aboutTelemetry to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutTelemetry.ftl",
        "toolkit/toolkit/about/aboutTelemetry.ftl",
        transforms_from(
		
"""
about-telemetry-ping-data-source = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.pingDataSource") }
about-telemetry-show-current-ping-data = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.showCurrentPingData") }
about-telemetry-show-archived-ping-data = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.showArchivedPingData") }
about-telemetry-show-subsession-data = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.showSubsessionData") }
about-telemetry-choose-ping = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.choosePing") }
about-telemetry-archive-ping-type = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.archivePingType") }
about-telemetry-archive-ping-header = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.archivePingHeader") }
about-telemetry-option-group-today = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.optionGroupToday") }
about-telemetry-option-group-yesterday = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.optionGroupYesterday") }
about-telemetry-option-group-older = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.optionGroupOlder") }
about-telemetry-previous-ping = { COPY("toolkit/chrome/global/aboutTelemetry.dtd", "aboutTelemetry.previousPing") }
about-telemetry-next-ping = { COPY("toolkit/chrome/global/aboutTelemetry.dtd", "aboutTelemetry.nextPing") }
about-telemetry-page-title = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.pageTitle") }
about-telemetry-more-information = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.moreInformations") }
about-telemetry-show-in-Firefox-json-viewer = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.showInFirefoxJsonViewer") }
about-telemetry-home-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.homeSection") }
about-telemetry-general-data-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.generalDataSection") }
about-telemetry-environment-data-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.environmentDataSection") }
about-telemetry-session-info-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.sessionInfoSection") }
about-telemetry-scalar-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.scalarsSection") }
about-telemetry-keyed-scalar-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.keyedScalarsSection") }
about-telemetry-histograms-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.histogramsSection") }
about-telemetry-keyed-histogram-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.keyedHistogramsSection") }
about-telemetry-events-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.eventsSection") }
about-telemetry-simple-measurements-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.simpleMeasurementsSection") }
about-telemetry-slow-sql-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.slowSqlSection") }
about-telemetry-addon-details-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.addonDetailsSection") }
about-telemetry-captured-stacks-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.capturedStacksSection") }
about-telemetry-late-writes-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.lateWritesSection") }
about-telemetry-raw-payload-section = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.rawPayloadSection") }
about-telemetry-raw = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.raw") }
about-telemetry-full-sql-warning = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.fullSqlWarning") }
about-telemetry-fetch-stack-symbols = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.fetchStackSymbols") }
about-telemetry-hide-stack-symbols = { COPY("toolkit/chrome/global/aboutTelemetry.dtd","aboutTelemetry.hideStackSymbols") }
about-telemetry-data-type =
    { $channel ->
        [release] { COPY("toolkit/chrome/global/aboutTelemetry.properties","releaseData") }
       *[prerelease] { COPY("toolkit/chrome/global/aboutTelemetry.properties","prereleaseData") }
    }
about-telemetry-upload-type =
    { $uploadcase ->
        [enabled] { COPY("toolkit/chrome/global/aboutTelemetry.properties","telemetryUploadEnabled") }
       *[disabled] { COPY("toolkit/chrome/global/aboutTelemetry.properties","telemetryUploadDisabled") }
    }
about-telemetry-filter-all-placeholder =
    .placeholder = { COPY("toolkit/chrome/global/aboutTelemetry.properties","filterAllPlaceholder") }
about-telemetry-current-ping-sidebar = { COPY("toolkit/chrome/global/aboutTelemetry.properties","currentPingSidebar") }
about-telemetry-telemetry-ping-type-all = { COPY("toolkit/chrome/global/aboutTelemetry.properties","telemetryPingTypeAll") }
about-telemetry-histogram-copy = { COPY("toolkit/chrome/global/aboutTelemetry.properties","histogramCopy") }
about-telemetry-slow-sql-main = { COPY("toolkit/chrome/global/aboutTelemetry.properties","slowSqlMain") }
about-telemetry-slow-sql-other = { COPY("toolkit/chrome/global/aboutTelemetry.properties","slowSqlOther") }
about-telemetry-slow-sql-hits = { COPY("toolkit/chrome/global/aboutTelemetry.properties","slowSqlHits") }
about-telemetry-slow-sql-average = { COPY("toolkit/chrome/global/aboutTelemetry.properties","slowSqlAverage") }
about-telemetry-slow-sql-statement = { COPY("toolkit/chrome/global/aboutTelemetry.properties","slowSqlStatement") }
about-telemetry-addon-table-id = { COPY("toolkit/chrome/global/aboutTelemetry.properties","addonTableID") }
about-telemetry-addon-table-details = { COPY("toolkit/chrome/global/aboutTelemetry.properties","addonTableDetails") }
about-telemetry-keys-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","keysHeader") }
about-telemetry-names-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","namesHeader") }
about-telemetry-values-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","valuesHeader") }
about-telemetry-stack-title = { COPY("toolkit/chrome/global/aboutTelemetry.properties","stackTitle") }
about-telemetry-memory-map-title = { COPY("toolkit/chrome/global/aboutTelemetry.properties","memoryMapTitle") }
about-telemetry-error-fetching-symbols = { COPY("toolkit/chrome/global/aboutTelemetry.properties","errorFetchingSymbols") }
about-telemetry-time-stamp-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","timestampHeader") }
about-telemetry-category-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","categoryHeader") }
about-telemetry-method-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","methodHeader") }
about-telemetry-object-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","objectHeader") }
about-telemetry-extra-header = { COPY("toolkit/chrome/global/aboutTelemetry.properties","extraHeader") }
"""
        )
    )
    ctx.add_transforms(
        "toolkit/toolkit/about/aboutTelemetry.ftl",
        "toolkit/toolkit/about/aboutTelemetry.ftl",
        [
			FTL.Message(
                id=FTL.Identifier("about-telemetry-firefox-data-doc"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.dtd",
                    "aboutTelemetry.firefoxDataDoc",
                    {
                        "<a>": FTL.TextElement('<a data-l10n-name="data-doc-link">'),
                    },
                )
            ),
			FTL.Message(
                id=FTL.Identifier("about-telemetry-telemetry-client-doc"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.dtd",
                    "aboutTelemetry.telemetryClientDoc",
                    {
                        "<a>": FTL.TextElement('<a data-l10n-name="client-doc-link">'),
                    },
                )
            ),
			FTL.Message(
                id=FTL.Identifier("about-telemetry-telemetry-dashboard"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.dtd",
                    "aboutTelemetry.telemetryDashboard",
                    {
                        "<a>": FTL.TextElement('<a data-l10n-name="dashboard-link">'),
                    },
                )
            ),
			FTL.Message(
                id=FTL.Identifier("about-telemetry-telemetry-probe-dictionary"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.dtd",
                    "aboutTelemetry.telemetryProbeDictionary",
                    {
                        "<a>": FTL.TextElement('<a data-l10n-name="probe-dictionary-link">'),
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-page-subtitle"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "pageSubtitle",
                    {
                        "%1$S": VARIABLE_REFERENCE("telemetryServerOwner"),
                        "%2$S": TERM_REFERENCE("brand-full-name"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-settings-explanation"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "settingsExplanation",
                    {
                        "%1$S": MESSAGE_REFERENCE("about-telemetry-data-type"),
                        "%2$S": CONCAT(
                        FTL.TextElement('<a data-l10n-name="upload-link">'),
                        MESSAGE_REFERENCE("about-telemetry-upload-type"),
                        FTL.TextElement("</a>")
                        ),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-ping-details"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "pingDetails",
                    {
                        "%1$S": CONCAT(
                        FTL.TextElement('<a data-l10n-name="ping-link">'),
                        COPY("toolkit/chrome/global/aboutTelemetry.properties","pingExplanationLink"),
                        FTL.TextElement("</a>")
                        ),
                        "%2$S": REPLACE(
                            "toolkit/chrome/global/aboutTelemetry.properties",
                            "namedPing",
                            {
                                "%1$S": VARIABLE_REFERENCE("name"),
                                "%2$S": VARIABLE_REFERENCE("timestamp"),
                            },
                            normalize_printf=True
                        )
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-ping-details-current"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "pingDetailsCurrent",
                    {
                        "%1$S": CONCAT(
                        FTL.TextElement('<a data-l10n-name="ping-link">'),
                        COPY("toolkit/chrome/global/aboutTelemetry.properties","pingExplanationLink"),
                        FTL.TextElement("</a>")
                        ),
                        "%2$S": COPY("toolkit/chrome/global/aboutTelemetry.properties","currentPing")
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-filter-placeholder"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"),
                        value=REPLACE(
                        "toolkit/chrome/global/aboutTelemetry.properties",
                        "filterPlaceholder",
                            {
                                "%1$S": VARIABLE_REFERENCE("selectedTitle"),
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-results-for-search"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "resultsForSearch",
                    {
                        "%1$S": VARIABLE_REFERENCE("searchTerms"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-no-search-results"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "noSearchResults",
                    {
                        "%1$S": VARIABLE_REFERENCE("sectionName"),
                        "%2$S": VARIABLE_REFERENCE("currentSearchText"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-no-search-results-all"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "noSearchResultsAll",
                    {
                        "%1$S": VARIABLE_REFERENCE("searchTerms"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-no-data-to-display"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "noDataToDisplay",
                    {
                        "%1$S": VARIABLE_REFERENCE("sectionName"),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("about-telemetry-addon-provider"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "addonProvider",
                    {
                        "%1$S": VARIABLE_REFERENCE("addonProvider"),
                    },
                    normalize_printf=True
                )
            ),

            FTL.Message(
                id=FTL.Identifier("about-telemetry-captured-stacks-title"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "captured-stacks-title",
                    {
                        "%1$S": VARIABLE_REFERENCE("stackKey"),
                        "%2$S": VARIABLE_REFERENCE("capturedStacksCount"),
                    },
                    normalize_printf=True
                )
            ),

            FTL.Message(
                id=FTL.Identifier("about-telemetry-late-writes-title"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutTelemetry.properties",
                    "late-writes-title",
                    {
                        "%1$S": VARIABLE_REFERENCE("lateWriteCount"),
                    },
                    normalize_printf=True
                )
            ),
        ]
    )
