# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import asyncio


async def settled_youtube(session, options):
    # nothing is done, we just settle here for 30 seconds
    await asyncio.sleep(options.get("sleep", 5))
    await asyncio.wait_for(
        session.get(
            "https://yttest.prod.mozaws.net/2020/main.html?"
            "test_type=playbackperf-widevine-sfr-h264-test&"
            "raptor=true&exclude=1,2&muted=true&command=run"
        ),
        timeout=120,
    )
    await asyncio.sleep(options.get("sleep", 30))
    return {}
