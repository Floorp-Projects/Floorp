# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from mozproxy.backends.mitm import MitmproxyDesktop, MitmproxyAndroid

_BACKENDS = {"mitmproxy": MitmproxyDesktop, "mitmproxy-android": MitmproxyAndroid}


def get_backend(name, *args, **kw):
    """Returns the class that implements the backend.

    Raises KeyError in case the backend does not exists.
    """
    return _BACKENDS[name](*args, **kw)
