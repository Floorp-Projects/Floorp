# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import asyncio


async def settled(session, options):
    # nothing is done, we just settle here for 30 seconds
    await asyncio.sleep(options.get("sleep", 30))
    return {}
