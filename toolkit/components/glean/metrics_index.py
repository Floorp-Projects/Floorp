# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ATTENTION: Changes to this file will need to be reflected in probe-scraper[1].
# This should happen automatically once a day.
# If something is unclear or data is not showing up in time
# you will need to file a bug in Data Platform and Tools :: General.
#
# [1] https://github.com/mozilla/probe-scraper

# Metrics that are sent by Gecko and everyone using Gecko
# (Firefox Desktop, Firefox for Android, Focus for Android, etc.).
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
gecko_metrics = [
    "browser/base/content/metrics.yaml",
    "dom/base/use_counter_metrics.yaml",
    "dom/media/metrics.yaml",
    "dom/media/webrtc/metrics.yaml",
    "dom/metrics.yaml",
    "gfx/metrics.yaml",
    "mobile/android/actors/metrics.yaml",
    "netwerk/metrics.yaml",
    "netwerk/protocol/http/metrics.yaml",
    "security/manager/ssl/metrics.yaml",
    "toolkit/components/cookiebanners/metrics.yaml",
    "toolkit/components/extensions/metrics.yaml",
    "toolkit/components/formautofill/metrics.yaml",
    "toolkit/components/glean/metrics.yaml",
    "toolkit/components/passwordmgr/metrics.yaml",
    "toolkit/components/pdfjs/metrics.yaml",
    "toolkit/components/processtools/metrics.yaml",
    "toolkit/components/reportbrokensite/metrics.yaml",
    "toolkit/components/resistfingerprinting/metrics.yaml",
    "toolkit/components/translations/metrics.yaml",
    "toolkit/mozapps/extensions/metrics.yaml",
    "toolkit/mozapps/handling/metrics.yaml",
    "xpcom/metrics.yaml",
]

# Metrics that are sent by Firefox Desktop
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
firefox_desktop_metrics = [
    "browser/components/metrics.yaml",
    "browser/components/migration/metrics.yaml",
    "browser/components/newtab/metrics.yaml",
    "browser/components/pocket/metrics.yaml",
    "browser/components/preferences/metrics.yaml",
    "browser/components/privatebrowsing/metrics.yaml",
    "browser/components/protocolhandler/metrics.yaml",
    "browser/components/search/metrics.yaml",
    "browser/components/shopping/metrics.yaml",
    "browser/components/urlbar/metrics.yaml",
    "browser/modules/metrics.yaml",
    "toolkit/components/crashes/metrics.yaml",
    "toolkit/components/nimbus/metrics.yaml",
    "toolkit/components/search/metrics.yaml",
    "toolkit/components/shopping/metrics.yaml",
    "toolkit/components/telemetry/dap/metrics.yaml",
    "toolkit/components/telemetry/metrics.yaml",
    "toolkit/modules/metrics.yaml",
    "toolkit/xre/metrics.yaml",
    "widget/cocoa/metrics.yaml",
]

# Metrics that are sent by the Firefox Desktop Background Update Task
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_update_metrics = [
    "toolkit/components/crashes/metrics.yaml",
    "toolkit/mozapps/update/metrics.yaml",
]

# Metrics that are sent by the Firefox Desktop Background Tasks
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_tasks_metrics = [
    "browser/components/metrics.yaml",
    "toolkit/components/backgroundtasks/metrics.yaml",
    "toolkit/components/crashes/metrics.yaml",
    "toolkit/mozapps/defaultagent/metrics.yaml",
]

# Test metrics
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
test_metrics = [
    "toolkit/components/glean/tests/test_metrics.yaml",
]

# The list of all Glean metrics.yaml files, relative to the top src dir.
# ONLY TO BE MODIFIED BY FOG PEERS!
metrics_yamls = sorted(
    set(
        gecko_metrics
        + firefox_desktop_metrics
        + background_update_metrics
        + background_tasks_metrics
        + test_metrics
    )
)

# Pings that are sent by Gecko and everyone using Gecko
# (Firefox Desktop, Firefox for Android, Focus for Android, etc.).
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
gecko_pings = [
    "dom/pings.yaml",
    "toolkit/components/glean/pings.yaml",
    "toolkit/components/reportbrokensite/pings.yaml",
]

# Pings that are sent by Firefox Desktop.
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
firefox_desktop_pings = [
    "browser/components/newtab/pings.yaml",
    "browser/components/pocket/pings.yaml",
    "browser/components/urlbar/pings.yaml",
    "toolkit/components/crashes/pings.yaml",
    "toolkit/components/telemetry/pings.yaml",
    "toolkit/modules/pings.yaml",
]

# Pings that are sent by the Firefox Desktop Background Update Task
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_update_pings = [
    "toolkit/components/crashes/pings.yaml",
    "toolkit/mozapps/update/pings.yaml",
]

# Pings that are sent by the Firefox Desktop Background Tasks
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_tasks_pings = [
    "toolkit/components/backgroundtasks/pings.yaml",
    "toolkit/components/crashes/pings.yaml",
    "toolkit/mozapps/defaultagent/pings.yaml",
]

# Test pings
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
test_pings = [
    "toolkit/components/glean/tests/test_pings.yaml",
]

# Map of app ids to lists of pings files for that app.
# Necessary to ensure different apps don't store data for unsent pings.
# Use the app id conjugation passed to initializeFOG (dotted.case).
pings_by_app_id = {
    "firefox.desktop": gecko_pings + firefox_desktop_pings + test_pings,
    "firefox.desktop.background.update": gecko_pings
    + background_update_pings
    + test_pings,
}

# The list of all Glean pings.yaml files, relative to the top src dir.
# ONLY TO BE MODIFIED BY FOG PEERS!
pings_yamls = sorted(
    set(
        gecko_pings
        + firefox_desktop_pings
        + background_update_pings
        + background_tasks_pings
        + test_pings
    )
)

# The list of tags that are allowed in the above to files, and their
# descriptions. Currently we restrict to a set scraped from bugzilla
# (via `./mach update-glean-tags`)
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
tags_yamls = [
    "toolkit/components/glean/tags.yaml",
]
