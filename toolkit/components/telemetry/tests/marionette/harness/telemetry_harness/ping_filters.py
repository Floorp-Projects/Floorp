# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class PingFilter(object):
    """Ping filter that accepts any pings."""

    def __call__(self, ping):
        return True


class DeletionRequestPingFilter(PingFilter):
    """Ping filter that accepts deletion-request pings."""

    def __call__(self, ping):
        if not super(DeletionRequestPingFilter, self).__call__(ping):
            return False

        return ping["type"] == "deletion-request"


class EventPingFilter(PingFilter):
    """Ping filter that accepts event pings."""

    def __call__(self, ping):
        if not super(EventPingFilter, self).__call__(ping):
            return False

        return ping["type"] == "event"


class FirstShutdownPingFilter(PingFilter):
    """Ping filter that accepts first-shutdown pings."""

    def __call__(self, ping):
        if not super(FirstShutdownPingFilter, self).__call__(ping):
            return False

        return ping["type"] == "first-shutdown"


class MainPingFilter(PingFilter):
    """Ping filter that accepts main pings."""

    def __call__(self, ping):
        if not super(MainPingFilter, self).__call__(ping):
            return False

        return ping["type"] == "main"


class MainPingReasonFilter(MainPingFilter):
    """Ping filter that accepts main pings that match the
    specified reason.
    """

    def __init__(self, reason):
        super(MainPingReasonFilter, self).__init__()
        self.reason = reason

    def __call__(self, ping):
        if not super(MainPingReasonFilter, self).__call__(ping):
            return False

        return ping["payload"]["info"]["reason"] == self.reason


ANY_PING = PingFilter()
DELETION_REQUEST_PING = DeletionRequestPingFilter()
EVENT_PING = EventPingFilter()
FIRST_SHUTDOWN_PING = FirstShutdownPingFilter()
MAIN_PING = MainPingFilter()
MAIN_SHUTDOWN_PING = MainPingReasonFilter("shutdown")
MAIN_ENVIRONMENT_CHANGE_PING = MainPingReasonFilter("environment-change")
