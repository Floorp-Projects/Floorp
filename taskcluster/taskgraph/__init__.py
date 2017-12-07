# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))

# Enable fast task generation for local debugging
# This is normally switched on via the --fast/-F flag to `mach taskgraph`
# Currently this skips toolchain task optimizations
fast = False
