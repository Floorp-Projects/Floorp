# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json


def json_response(func):
    """ Translates results of 'func' into a JSON response. """
    def wrap(*a, **kw):
        (code, data) = func(*a, **kw)
        json_data = json.dumps(data)
        return (code, {'Content-type': 'application/json',
                       'Content-Length': len(json_data)}, json_data)

    return wrap
