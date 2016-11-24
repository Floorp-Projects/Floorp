# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import errors


DEFAULT_SCRIPT_TIMEOUT = 30
DEFAULT_PAGE_LOAD_TIMEOUT = 300
DEFAULT_IMPLICIT_WAIT_TIMEOUT = 0


class Timeouts(object):
    """Manage timeout settings in the Marionette session.

    Usage::

        marionette = Marionette(...)
        marionette.start_session()
        marionette.timeout.page_load = 10
        marionette.timeout.page_load
        # => 10

    """

    def __init__(self, marionette):
        self._marionette = marionette

    def _set(self, name, sec):
        ms = sec * 1000
        try:
            self._marionette._send_message("setTimeouts", {name: ms})
        except errors.UnknownCommandException:
            # remove when 55 is stable
            self._marionette._send_message("timeouts", {"type": name, "ms": ms})

    def _get(self, name):
        ms = self._marionette._send_message("getTimeouts", key=name)
        return ms / 1000

    @property
    def script(self):
        """Get the session's script timeout.  This specifies the time
        to wait for injected scripts to finished before interrupting
        them. It is by default 30 seconds.

        """
        return self._get("script")

    @script.setter
    def script(self, sec):
        """Set the session's script timeout.  This specifies the time
        to wait for injected scripts to finish before interrupting them.

        """
        self._set("script", sec)

    @property
    def page_load(self):
        """Get the session's page load timeout.  This specifies the time
        to wait for the page loading to complete.  It is by default 5
        minutes (or 300 seconds).

        """
        return self._get("page load")

    @page_load.setter
    def page_load(self, sec):
        """Set the session's page load timeout.  This specifies the time
        to wait for the page loading to complete.

        """
        self._set("page load", sec)

    @property
    def implicit(self):
        """Get the session's implicit wait timeout.  This specifies the
        time to wait for the implicit element location strategy when
        retrieving elements.  It is by default disabled (0 seconds).

        """
        return self._get("implicit")

    @implicit.setter
    def implicit(self, sec):
        """Set the session's implicit wait timeout.  This specifies the
        time to wait for the implicit element location strategy when
        retrieving elements.

        """
        self._set("implicit", sec)

    def reset(self):
        """Resets timeouts to their default values."""
        self.script = DEFAULT_SCRIPT_TIMEOUT
        self.page_load = DEFAULT_PAGE_LOAD_TIMEOUT
        self.implicit = DEFAULT_IMPLICIT_WAIT_TIMEOUT
