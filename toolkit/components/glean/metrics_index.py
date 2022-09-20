# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ATTENTION: If you make changes to this file you will need to file a bug in
# Data Platform and Tools :: General asking for the changes to be reflected
# in the data pipeline. Otherwise bad things will happen to good people:
# any new metrics files' metrics will not get columns in any datasets.

# The list of all Glean metrics.yaml files, relative to the top src dir.
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
metrics_yamls = [
    "browser/base/content/metrics.yaml",
    "browser/components/metrics.yaml",
    "browser/components/newtab/metrics.yaml",
    "browser/components/search/metrics.yaml",
    "browser/modules/metrics.yaml",
    "dom/media/metrics.yaml",
    "dom/metrics.yaml",
    "gfx/metrics.yaml",
    "netwerk/metrics.yaml",
    "netwerk/protocol/http/metrics.yaml",
    "toolkit/components/extensions/metrics.yaml",
    "toolkit/components/glean/metrics.yaml",
    "toolkit/components/glean/tests/test_metrics.yaml",
    "toolkit/components/nimbus/metrics.yaml",
    "toolkit/components/pdfjs/metrics.yaml",
    "toolkit/components/processtools/metrics.yaml",
    "toolkit/components/search/metrics.yaml",
    "toolkit/components/telemetry/metrics.yaml",
    "toolkit/mozapps/update/metrics.yaml",
    "toolkit/xre/metrics.yaml",
]

# The list of all Glean pings.yaml files, relative to the top src dir.
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
pings_yamls = [
    "browser/components/newtab/pings.yaml",
    "toolkit/components/glean/pings.yaml",
    "toolkit/components/glean/tests/test_pings.yaml",
    "toolkit/components/telemetry/pings.yaml",
    "toolkit/mozapps/update/pings.yaml",
]

# The list of tags that are allowed in the above to files, and their
# descriptions. Currently we restrict to a set scraped from bugzilla
# (via `./mach update-glean-tags`)
# Order is lexicographical, enforced by t/c/glean/tests/pytest/test_yaml_indices.py
tags_yamls = [
    "toolkit/components/glean/tags.yaml",
]
