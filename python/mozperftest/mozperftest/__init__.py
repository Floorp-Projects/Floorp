# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozlog

from mozperftest.argparser import PerftestArgumentParser  # noqa
from mozperftest.metadata import Metadata  # noqa
from mozperftest.environment import MachEnvironment  # noqa

logger = mozlog.commandline.setup_logging("mozperftest", {})
