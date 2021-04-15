# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The list of all Glean metrics.yaml files, relative to the top src dir.
# New additions should be added to the bottom of the list.
metrics_yamls = [
    "toolkit/components/glean/metrics.yaml",
    "toolkit/components/glean/test_metrics.yaml",
    "toolkit/mozapps/update/metrics.yaml",
]

# The list of all Glean pings.yaml files, relative to the top src dir.
# New additions should be added to the bottom of the list.
pings_yamls = [
    "toolkit/components/glean/pings.yaml",
    "toolkit/components/glean/test_pings.yaml",
    "toolkit/mozapps/update/pings.yaml",
]
