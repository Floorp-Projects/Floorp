# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Python port of the ms.js node module this is not a direct port some things are
# more complicated or less precise and we lean on time delta here.

import re
import datetime

PATTERN=re.compile(
    '((?:\d+)?\.?\d+) *([a-z]+)'
)

def seconds(value):
    return datetime.timedelta(seconds=int(value))

def minutes(value):
    return datetime.timedelta(minutes=int(value))

def hours(value):
    return datetime.timedelta(hours=int(value))

def days(value):
    return datetime.timedelta(days=int(value))

def months(value):
    # See warning in years(), below
    return datetime.timedelta(days=int(value) * 30)

def years(value):
    # Warning here "years" are vague don't use this for really sensitive date
    # computation the idea is to give you a absolute amount of time in the
    # future which is not the same thing as "precisely on this date next year"
    return datetime.timedelta(days=int(value) * 365)

ALIASES = {}
ALIASES['seconds'] = ALIASES['second'] = ALIASES['s'] = seconds
ALIASES['minutes'] = ALIASES['minute'] = ALIASES['min'] = minutes
ALIASES['hours'] = ALIASES['hour'] = ALIASES['h'] = hours
ALIASES['days'] = ALIASES['day'] = ALIASES['d'] = days
ALIASES['months'] = ALIASES['month'] = ALIASES['mo'] = months
ALIASES['years'] = ALIASES['year'] = ALIASES['y'] = years

class InvalidString(Exception):
    pass

class UnknownTimeMeasurement(Exception):
    pass

def value_of(input_str):
    '''
    Convert a string to a json date in the future
    :param str input_str: (ex: 1d, 2d, 6years, 2 seconds)
    :returns: Unit given in seconds
    '''

    matches = PATTERN.search(input_str)

    if matches is None or len(matches.groups()) < 2:
        raise InvalidString("'{}' is invalid string".format(input_str))

    value, unit = matches.groups()

    if unit not in ALIASES:
        raise UnknownTimeMeasurement(
            '{} is not a valid time measure use one of {}'.format(unit,
                sorted(ALIASES.keys()))
        )

    return ALIASES[unit](value)

def json_time_from_now(input_str, now=None):
    '''
    :param str input_str: Input string (see value of)
    :param datetime now: Optionally set the definition of `now`
    :returns: JSON string representation of time in future.
    '''

    if now is None:
        now = datetime.datetime.utcnow()

    time = now + value_of(input_str)

    # Sorta a big hack but the json schema validator for date does not like the
    # ISO dates until 'Z' (for timezone) is added...
    return time.isoformat() + 'Z'

def current_json_time():
    '''
    :returns: JSON string representation of the current time.
    '''

    return datetime.datetime.utcnow().isoformat() + 'Z'

