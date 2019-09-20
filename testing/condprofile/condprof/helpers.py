# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Helpers to build scenarii
"""
from condprof.util import ERROR


class TabSwitcher:
    """ Helper used to create tabs and circulate in them.
    """

    def __init__(self, session, options):
        self.handles = None
        self.current = 0
        self.session = session
        self._max = options.get("max_urls", 10)
        self.platform = options.get("platform", "")
        self.num_tabs = self._max >= 100 and 100 or self._max
        self._mobile = "fenix" in self.platform or "gecko" in self.platform

    async def create_windows(self):
        # on mobile we don't use tabs for now
        # see https://bugzil.la/1559120
        if self._mobile:
            return
        # creating tabs
        for i in range(self.num_tabs):
            # see https://github.com/HDE/arsenic/issues/71
            await self.session._request(
                url="/window/new", method="POST", data={"type": "tab"}
            )

    async def switch(self):
        if self._mobile:
            return
        try:
            if self.handles is None:
                self.handles = await self.session.get_window_handles()
                self.current = 0
        except Exception:
            ERROR("Could not get window handles")
            return

        if self.current not in self.handles:
            ERROR("Handle %s not in current set of windows" % str(self.current))
            return

        handle = self.handles[self.current]
        if self.current == len(self.handles) - 1:
            self.current = 0
        else:
            self.current += 1
        try:
            await self.session.switch_to_window(handle)
        except Exception:
            ERROR("Could not switch to handle %s" % str(handle))
