# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

about-telemetry-ping-data-source = Ping data source:
about-telemetry-show-current-data = Current data
about-telemetry-show-archived-ping-data = Archived ping data
about-telemetry-show-subsession-data = Show subsession data
about-telemetry-choose-ping = Choose ping:
about-telemetry-archive-ping-type = Ping Type
about-telemetry-archive-ping-header = Ping
about-telemetry-option-group-today = Today
about-telemetry-option-group-yesterday = Yesterday
about-telemetry-option-group-older = Older
about-telemetry-previous-ping = <<
about-telemetry-next-ping = >>
about-telemetry-page-title = Telemetry Data
about-telemetry-current-store = Current Store:
about-telemetry-more-information = Looking for more information?
about-telemetry-firefox-data-doc = The <a data-l10n-name="data-doc-link">Firefox Data Documentation</a> contains guides about how to work with our data tools.
about-telemetry-telemetry-client-doc = The <a data-l10n-name="client-doc-link">Firefox Telemetry client documentation</a> includes definitions for concepts, API documentation and data references.
about-telemetry-telemetry-dashboard = The <a data-l10n-name="dashboard-link">Telemetry dashboards</a> allow you to visualize the data Mozilla receives via Telemetry.
about-telemetry-telemetry-probe-dictionary = The <a data-l10n-name="probe-dictionary-link">Probe Dictionary</a> provides details and descriptions for the probes collected by Telemetry.
about-telemetry-show-in-Firefox-json-viewer = Open in the JSON viewer
about-telemetry-home-section = Home
about-telemetry-general-data-section = General Data
about-telemetry-environment-data-section = Environment Data
about-telemetry-session-info-section = Session Information
about-telemetry-scalar-section = Scalars
about-telemetry-keyed-scalar-section = Keyed Scalars
about-telemetry-histograms-section = Histograms
about-telemetry-keyed-histogram-section = Keyed Histograms
about-telemetry-events-section = Events
about-telemetry-simple-measurements-section = Simple Measurements
about-telemetry-slow-sql-section = Slow SQL Statements
about-telemetry-addon-details-section = Add-on Details
about-telemetry-captured-stacks-section = Captured Stacks
about-telemetry-late-writes-section = Late Writes
about-telemetry-raw-payload-section = Raw Payload
about-telemetry-raw = Raw JSON
about-telemetry-full-sql-warning = NOTE: Slow SQL debugging is enabled. Full SQL strings may be displayed below but they will not be submitted to Telemetry.
about-telemetry-fetch-stack-symbols = Fetch function names for stacks
about-telemetry-hide-stack-symbols = Show raw stack data
# Selects the correct release version
# Variables:
#   $channel (String): represents the corresponding release data string
about-telemetry-data-type =
    { $channel ->
        [release] release data
       *[prerelease] pre-release data
    }
# Selects the correct upload string
# Variables:
#   $uploadcase (String): represents a corresponding upload string
about-telemetry-upload-type =
    { $uploadcase ->
        [enabled] enabled
       *[disabled] disabled
    }
# Example Output: 1 sample, average = 0, sum = 0
# Variables:
#   $sampleCount (Integer): amount of histogram samples
#   $prettyAverage (Integer): average of histogram samples
#   $sum (Integer): sum of histogram samples
about-telemetry-histogram-stats =
    { $sampleCount ->
        [one] { $sampleCount } sample, average = { $prettyAverage }, sum = { $sum }
       *[other] { $sampleCount } samples, average = { $prettyAverage }, sum = { $sum }
    }
# Variables:
#   $telemetryServerOwner (String): the value of the toolkit.telemetry.server_owner preference. Typically "Mozilla"
about-telemetry-page-subtitle = This page shows the information about performance, hardware, usage and customizations collected by Telemetry. This information is submitted to { $telemetryServerOwner } to help improve { -brand-full-name }.
about-telemetry-settings-explanation = Telemetry is collecting { about-telemetry-data-type } and upload is <a data-l10n-name="upload-link">{ about-telemetry-upload-type }</a>.
# Variables:
#   $name (String): ping name, e.g. “saved-session”
#   $timeStamp (String): ping localized timestamp, e.g. “2017/07/08 10:40:46”
about-telemetry-ping-details = Each piece of information is sent bundled into “<a data-l10n-name="ping-link">pings</a>”. You are looking at the { $name }, { $timestamp } ping.
about-telemetry-data-details-current = Each piece of information is sent bundled into “<a data-l10n-name="ping-link">pings</a>“. You are looking at the current data.
# string used as a placeholder for the search field
# More info about it can be found here:
# https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/main-ping.html
# Variables:
#   $selectedTitle (String): the section name from the structure of the ping.
about-telemetry-filter-placeholder =
    .placeholder = Find in { $selectedTitle }
about-telemetry-filter-all-placeholder =
    .placeholder = Find in all sections
# Variables:
#   $searchTerms (String): the searched terms
about-telemetry-results-for-search = Results for “{ $searchTerms }”
# More info about it can be found here: https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/main-ping.html
# Variables:
#   $sectionName (String): the section name from the structure of the ping.
#   $currentSearchText (String): the current text in the search input
about-telemetry-no-search-results = Sorry! There are no results in { $sectionName } for “{ $currentSearchText }”
# Variables:
#   $searchTerms (String): the searched terms
about-telemetry-no-search-results-all = Sorry! There are no results in any sections for “{ $searchTerms }”
# This message is displayed when a section is empty.
# Variables:
#   $sectionName (String): is replaced by the section name.
about-telemetry-no-data-to-display = Sorry! There is currently no data available in “{ $sectionName }”
# used as a tooltip for the “current” ping title in the sidebar
about-telemetry-current-data-sidebar = current data
# used in the “Ping Type” select
about-telemetry-telemetry-ping-type-all = all
# button label to copy the histogram
about-telemetry-histogram-copy = Copy
# these strings are used in the “Slow SQL Statements” section
about-telemetry-slow-sql-main = Slow SQL Statements on Main Thread
about-telemetry-slow-sql-other = Slow SQL Statements on Helper Threads
about-telemetry-slow-sql-hits = Hits
about-telemetry-slow-sql-average = Avg. Time (ms)
about-telemetry-slow-sql-statement = Statement
# these strings are used in the “Add-on Details” section
about-telemetry-addon-table-id = Add-on ID
about-telemetry-addon-table-details = Details
# Variables:
#   $addonProvider (String): the name of an Add-on Provider (e.g. “XPI”, “Plugin”)
about-telemetry-addon-provider = { $addonProvider } Provider
about-telemetry-keys-header = Property
about-telemetry-names-header = Name
about-telemetry-values-header = Value
# Variables:
#   $stackKey (String): the string key for this stack
#   $capturedStacksCount (Integer):  the number of times this stack was captured
about-telemetry-captured-stacks-title = { $stackKey } (capture count: { $capturedStacksCount })
# Variables:
#   $lateWriteCount (Integer): the number of the late writes
about-telemetry-late-writes-title = Late Write #{ $lateWriteCount }
about-telemetry-stack-title = Stack:
about-telemetry-memory-map-title = Memory map:
about-telemetry-error-fetching-symbols = An error occurred while fetching symbols. Check that you are connected to the Internet and try again.
about-telemetry-time-stamp-header = timestamp
about-telemetry-category-header = category
about-telemetry-method-header = method
about-telemetry-object-header = object
about-telemetry-extra-header = extra
about-telemetry-origin-section = Origin Telemetry
about-telemetry-origin-origin = origin
about-telemetry-origin-count = count
# Variables:
#   $telemetryServerOwner (String): the value of the toolkit.telemetry.server_owner preference. Typically "Mozilla"
about-telemetry-origins-explanation = <a data-l10n-name="origin-doc-link">Firefox Origin Telemetry</a> encodes data before it is sent so that { $telemetryServerOwner } can count things, but not know whether or not any given { -brand-product-name } contributed to that count. (<a data-l10n-name="prio-blog-link">learn more</a>)
# Variables:
#  $process (String): type of process in subsection headers ( e.g. "content", "parent" )
about-telemetry-process = { $process } process
