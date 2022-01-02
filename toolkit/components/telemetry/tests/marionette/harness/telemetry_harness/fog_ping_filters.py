# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class FOGPingFilter(object):
    """Ping filter that accepts any FOG pings."""

    def __call__(self, ping):
        return True


class FOGDocTypePingFilter(FOGPingFilter):
    """Ping filter that accepts FOG pings that match the doc-type."""

    def __init__(self, doc_type):
        super(FOGDocTypePingFilter, self).__init__()
        self.doc_type = doc_type

    def __call__(self, ping):
        if not super(FOGDocTypePingFilter, self).__call__(ping):
            return False

        # Verify that the given ping was submitted to the URL for the doc_type
        return ping["request_url"]["doc_type"] == self.doc_type


FOG_BACKGROUND_UPDATE_PING = FOGDocTypePingFilter("background-update")
FOG_BASELINE_PING = FOGDocTypePingFilter("baseline")
FOG_DELETION_REQUEST_PING = FOGDocTypePingFilter("deletion-request")
FOG_ONE_PING_ONLY_PING = FOGDocTypePingFilter("one-ping-only")
