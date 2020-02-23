# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import asyncio

from condprof.scenarii.heavy import URL_LIST
from condprof.util import LOG, get_credentials
from condprof.helpers import TabSwitcher, execute_async_script


with open(os.path.join(os.path.dirname(__file__), "sync.js")) as f:
    SYNC_SCRIPT = f.read()


async def synced(session, options):
    username, password = get_credentials()
    if username is None:
        raise ValueError("The synced scenario needs an fxa username and" " password")
    metadata = {}
    max = options.get("max_urls", 150)
    tabs = TabSwitcher(session, options)
    await tabs.create_windows()
    visited = 0

    for current, url in enumerate(URL_LIST):
        LOG("%d/%d %s" % (current + 1, max, url))
        retries = 0
        while retries < 3:
            try:
                await asyncio.wait_for(session.get(url), 5)
                visited += 1
                break
            except asyncio.TimeoutError:
                retries += 1

        if max != -1 and current + 1 == max:
            break

        # switch to the next tab
        await tabs.switch()

    # now that we've visited all pages, we want to upload to FXSync
    LOG("Syncing profile to FxSync")
    LOG("Username is %s, password is %s" % (username, password))
    script_res = await execute_async_script(
        session, SYNC_SCRIPT, username, password, "https://accounts.stage.mozaws.net"
    )
    if script_res is None:
        script_res = {}
    metadata["logs"] = script_res.get("logs", {})
    metadata["result"] = script_res.get("result", 0)
    metadata["result_message"] = script_res.get("result_message", "SUCCESS")
    metadata["visited_url"] = visited
    return metadata
