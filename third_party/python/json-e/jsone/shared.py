from __future__ import absolute_import, print_function, unicode_literals

import re
import datetime

# this will be overridden in tests
utcnow = datetime.datetime.utcnow


class DeleteMarker:
    pass


class JSONTemplateError(Exception):
    pass


# Regular expression matching: X days Y hours Z minutes
# todo: support hr, wk, yr
FROMNOW_RE = re.compile(''.join([
    '^(\s*(?P<years>\d+)\s*y(ears?)?)?',
    '(\s*(?P<months>\d+)\s*mo(nths?)?)?',
    '(\s*(?P<weeks>\d+)\s*w(eeks?)?)?',
    '(\s*(?P<days>\d+)\s*d(ays?)?)?',
    '(\s*(?P<hours>\d+)\s*h(ours?)?)?',
    '(\s*(?P<minutes>\d+)\s*m(in(utes?)?)?)?\s*',
    '(\s*(?P<seconds>\d+)\s*s(ec(onds?)?)?)?\s*$',
]))


def fromNow(offset):
    # copied from taskcluster-client.py
    # We want to handle past dates as well as future
    future = True
    offset = offset.lstrip()
    if offset.startswith('-'):
        future = False
        offset = offset[1:].lstrip()
    if offset.startswith('+'):
        offset = offset[1:].lstrip()

    # Parse offset
    m = FROMNOW_RE.match(offset)
    if m is None:
        raise ValueError("offset string: '%s' does not parse" % offset)

    # In order to calculate years and months we need to calculate how many days
    # to offset the offset by, since timedelta only goes as high as weeks
    days = 0
    hours = 0
    minutes = 0
    seconds = 0
    if m.group('years'):
        # forget leap years, a year is 365 days
        years = int(m.group('years'))
        days += 365 * years
    if m.group('months'):
        # assume "month" means 30 days
        months = int(m.group('months'))
        days += 30 * months
    days += int(m.group('days') or 0)
    hours += int(m.group('hours') or 0)
    minutes += int(m.group('minutes') or 0)
    seconds += int(m.group('seconds') or 0)

    # Offset datetime from utc
    delta = datetime.timedelta(
        weeks=int(m.group('weeks') or 0),
        days=days,
        hours=hours,
        minutes=minutes,
        seconds=seconds,
    )

    return stringDate(utcnow() + delta if future else utcnow() - delta)


datefmt_re = re.compile(r'(\.[0-9]{3})[0-9]*(\+00:00)?')


def stringDate(date):
    # Convert to isoFormat
    string = date.isoformat()
    string = datefmt_re.sub(r'\1Z', string)
    return string

# the base class for strings, regardless of python version
try:
    string = basestring
except NameError:
    string = str
