# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from condprof.scenarii.full import full
from condprof.scenarii.settled import settled
from condprof.scenarii.settled2 import settled2


scenarii = {"full": full, "settled": settled, "settled2": settled2}
