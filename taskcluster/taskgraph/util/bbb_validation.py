# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import requests

from mozbuild.util import memoize
from redo import retry
from requests import exceptions

ALLTHETHINGS_URL = "https://secure.pub.build.mozilla.org/builddata/reports/allthethings.json.gz"

logger = logging.getLogger(__name__)


def fetch_all_the_things():
    response = retry(requests.get, attempts=2, sleeptime=10,
                     args=(ALLTHETHINGS_URL,),
                     kwargs={'timeout': 60})
    return response.content


@memoize
def valid_bbb_builders():
    try:
        allthethings = fetch_all_the_things()
        builders = set(json.loads(allthethings).get('builders', {}).keys())
        return builders

    # In the event of request times out, requests will raise a TimeoutError.
    except exceptions.Timeout:
        logger.warning("Timeout fetching list of buildbot builders.")

    # In the event of a network problem (e.g. DNS failure, refused connection, etc),
    # requests will raise a ConnectionError.
    except exceptions.ConnectionError:
        logger.warning("Connection Error while fetching list of buildbot builders")

    # We just print the error out as a debug message if we failed to catch the exception above
    except exceptions.RequestException as error:
        logger.warning(error)

    # When we get invalid JSON (i.e. 500 error), it results in a ValueError
    except ValueError as error:
        logger.warning("Invalid JSON, possible server error: {}".format(error))

    # None returned to treat as "All Builders Valid"
    return None
