# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" This is a *TEMPORARY* file that stubs glean metrics using the events API.
This will be removed when bug 1497800 lands and the glean_parsers package
is published on PyPi.
"""

import sys
import os

with open(sys.argv[1], 'r') as my_file:
    # This reads the metric file, just to make sure we're able to find it.
    print(my_file.read())
    # Write the kotlin file.

DATA = """
package GleanMetrics

import mozilla.components.service.glean.EventMetricType

object BrowserEngagement {
     /**
       * Automatically generated comment containing metric
       * description and useful metadata.
       */
    val click: EventMetricType by lazy {
        EventMetricType(
            applicationProperty = true,
            disabled = false,
            group = "ui",
            name = "click",
            sendInPings = listOf("store1"),
            userProperty = false
        )
    }
}"""

os.makedirs(sys.argv[2], exist_ok=True)
with open(os.path.join(sys.argv[2], "BrowserEngagementMetrics.kt"), 'w') as out_file:
    out_file.write(DATA)
