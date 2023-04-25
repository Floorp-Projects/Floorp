# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from taskgraph.morph import _SCOPE_SUMMARY_REGEXPS

# TODO Bug 1805651: let taskgraph allow us to specify this route
_SCOPE_SUMMARY_REGEXPS.append(
    re.compile(r"(index:insert-task:mobile\.v3.[^.]*\.).*")
)
