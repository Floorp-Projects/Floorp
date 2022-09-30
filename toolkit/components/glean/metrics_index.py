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
    "dom/media/metrics.yaml",
    "dom/metrics.yaml",
    "gfx/metrics.yaml",
    "netwerk/metrics.yaml",
    "netwerk/protocol/http/metrics.yaml",
    "toolkit/components/glean/metrics.yaml",
    "toolkit/components/processtools/metrics.yaml",
]

# Metrics that are sent by Firefox Desktop
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
firefox_desktop_metrics = [
    "browser/components/metrics.yaml",
    "browser/components/newtab/metrics.yaml",
    "browser/components/search/metrics.yaml",
    "browser/modules/metrics.yaml",
    "toolkit/components/extensions/metrics.yaml",
    "toolkit/components/nimbus/metrics.yaml",
    "toolkit/components/pdfjs/metrics.yaml",
    "toolkit/components/search/metrics.yaml",
    "toolkit/components/telemetry/metrics.yaml",
    "toolkit/xre/metrics.yaml",
]

# Metrics that are sent by the Firefox Desktop Background Update Task
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_update_metrics = [
    "toolkit/mozapps/update/metrics.yaml",
]

# Test metrics
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
test_metrics = [
    "toolkit/components/glean/tests/test_metrics.yaml",
]

# The list of all Glean metrics.yaml files, relative to the top src dir.
# ONLY TO BE MODIFIED BY FOG PEERS!
metrics_yamls = (
    gecko_metrics + firefox_desktop_metrics + background_update_metrics + test_metrics
)

# Pings that are sent by Gecko and everyone using Gecko
# (Firefox Desktop, Firefox for Android, Focus for Android, etc.).
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
gecko_pings = [
    "toolkit/components/glean/pings.yaml",
]

# Pings that are sent by Firefox Desktop.
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
firefox_desktop_pings = [
    "browser/components/newtab/pings.yaml",
    "toolkit/components/telemetry/pings.yaml",
]

# Pings that are sent by the Firefox Desktop Background Update Task
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
background_update_pings = [
    "toolkit/mozapps/update/pings.yaml",
]

# Test pings
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
test_pings = [
    "toolkit/components/glean/tests/test_pings.yaml",
]

# The list of all Glean pings.yaml files, relative to the top src dir.
# ONLY TO BE MODIFIED BY FOG PEERS!
pings_yamls = gecko_pings + firefox_desktop_pings + background_update_pings + test_pings

# The list of tags that are allowed in the above to files, and their
# descriptions. Currently we restrict to a set scraped from bugzilla
# (via `./mach update-glean-tags`)
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
tags_yamls = [
    "toolkit/components/glean/tags.yaml",
]
