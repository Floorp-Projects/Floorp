"""Parse, manipulate and render cookies in a convenient way.

Copyright (c) 2011-2014, Sasha Hart.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
__version__ = "2.2.1"
import re
import datetime
import logging
import sys
from unicodedata import normalize
if sys.version_info >= (3, 0, 0):  # pragma: no cover
    from urllib.parse import (
        quote as _default_quote, unquote as _default_unquote)
    basestring = str
    long = int
else:  # pragma: no cover
    from urllib import (
        quote as _default_quote, unquote as _default_unquote)


def _total_seconds(td):
    """Wrapper to work around lack of .total_seconds() method in Python 3.1.
    """
    if hasattr(td, "total_seconds"):
        return td.total_seconds()
    return td.days * 3600 * 24 + td.seconds + td.microseconds / 100000.0

# see test_encoding_assumptions for how these magical safe= parms were figured
# out. the differences are because of what cookie-octet may contain
# vs the more liberal spec for extension-av
default_cookie_quote = lambda item: _default_quote(
    item, safe='!#$%&\'()*+/:<=>?@[]^`{|}~')

default_extension_quote = lambda item: _default_quote(
    item, safe=' !"#$%&\'()*+,/:<=>?@[\\]^`{|}~')

default_unquote = _default_unquote


def _report_invalid_cookie(data):
    "How this module logs a bad cookie when exception suppressed"
    logging.error("invalid Cookie: %r", data)


def _report_unknown_attribute(name):
    "How this module logs an unknown attribute when exception suppressed"
    logging.error("unknown Cookie attribute: %r", name)


def _report_invalid_attribute(name, value, reason):
    "How this module logs a bad attribute when exception suppressed"
    logging.error("invalid Cookie attribute (%s): %r=%r", reason, name, value)


class CookieError(Exception):
    """Base class for this module's exceptions, so you can catch them all if
    you want to.
    """
    def __init__(self):
        Exception.__init__(self)


class InvalidCookieError(CookieError):
    """Raised when attempting to parse or construct a cookie which is
    syntactically invalid (in any way that has possibly serious implications).
    """
    def __init__(self, data=None, message=""):
        CookieError.__init__(self)
        self.data = data
        self.message = message

    def __str__(self):
        return '%r %r' % (self.message, self.data)


class InvalidCookieAttributeError(CookieError):
    """Raised when setting an invalid attribute on a Cookie.
    """
    def __init__(self, name, value, reason=None):
        CookieError.__init__(self)
        self.name = name
        self.value = value
        self.reason = reason

    def __str__(self):
        prefix = ("%s: " % self.reason) if self.reason else ""
        if self.name is None:
            return '%s%r' % (prefix, self.value)
        return '%s%r = %r' % (prefix, self.name, self.value)


class Definitions(object):
    """Namespace to hold definitions used in cookie parsing (mostly pieces of
    regex).

    These are separated out for individual testing against examples and RFC
    grammar, and kept here to avoid cluttering other namespaces.
    """
    # Most of the following are set down or cited in RFC 6265 4.1.1

    # This is the grammar's 'cookie-name' defined as 'token' per RFC 2616 2.2.
    COOKIE_NAME = r"!#$%&'*+\-.0-9A-Z^_`a-z|~"

    # 'cookie-octet' - as used twice in definition of 'cookie-value'
    COOKIE_OCTET = r"\x21\x23-\x2B\--\x3A\x3C-\x5B\]-\x7E"

    # extension-av - also happens to be a superset of cookie-av and path-value
    EXTENSION_AV = """ !"#$%&\\\\'()*+,\-./0-9:<=>?@A-Z[\\]^_`a-z{|}~"""

    # This is for the first pass parse on a Set-Cookie: response header. It
    # includes cookie-value, cookie-pair, set-cookie-string, cookie-av.
    # extension-av is used to extract the chunk containing variable-length,
    # unordered attributes. The second pass then uses ATTR to break out each
    # attribute and extract it appropriately.
    # As compared with the RFC production grammar, it is must more liberal with
    # space characters, in order not to break on data made by barbarians.
    SET_COOKIE_HEADER = """(?x) # Verbose mode
        ^(?:Set-Cookie:[ ]*)?
        (?P<name>[{name}:]+)
        [ ]*=[ ]*

        # Accept anything in quotes - this is not RFC 6265, but might ease
        # working with older code that half-heartedly works with 2965. Accept
        # spaces inside tokens up front, so we can deal with that error one
        # cookie at a time, after this first pass.
        (?P<value>(?:"{value}*")|(?:[{cookie_octet} ]*))
        [ ]*

        # Extract everything up to the end in one chunk, which will be broken
        # down in the second pass. Don't match if there's any unexpected
        # garbage at the end (hence the \Z; $ matches before newline).
        (?P<attrs>(?:;[ ]*[{cookie_av}]+)*)
        """.format(name=COOKIE_NAME, cookie_av=EXTENSION_AV + ";",
                   cookie_octet=COOKIE_OCTET, value="[^;]")

    # Now we specify the individual patterns for the attribute extraction pass
    # of Set-Cookie parsing (mapping to *-av in the RFC grammar). Things which
    # don't match any of these but are in extension-av are simply ignored;
    # anything else should be rejected in the first pass (SET_COOKIE_HEADER).

    # Max-Age attribute. These are digits, they are expressed this way
    # because that is how they are expressed in the RFC.
    MAX_AGE_AV = "Max-Age=(?P<max_age>[\x30-\x39]+)"

    # Domain attribute; a label is one part of the domain
    LABEL = '{let_dig}(?:(?:{let_dig_hyp}+)?{let_dig})?'.format(
            let_dig="[A-Za-z0-9]", let_dig_hyp="[0-9A-Za-z\-]")
    DOMAIN = "\.?(?:{label}\.)*(?:{label})".format(label=LABEL)
    # Parse initial period though it's wrong, as RFC 6265 4.1.2.3
    DOMAIN_AV = "Domain=(?P<domain>{domain})".format(domain=DOMAIN)

    # Path attribute. We don't take special care with quotes because
    # they are hardly used, they don't allow invalid characters per RFC 6265,
    # and " is a valid character to occur in a path value anyway.
    PATH_AV = 'Path=(?P<path>[%s]+)' % EXTENSION_AV

    # Expires attribute. This gets big because of date parsing, which needs to
    # support a large range of formats, so it's broken down into pieces.

    # Generate a mapping of months to use in render/parse, to avoid
    # localizations which might be produced by strftime (e.g. %a -> Mayo)
    month_list = ["January", "February", "March", "April", "May", "June",
                  "July", "August", "September", "October", "November",
                  "December"]
    month_abbr_list = [item[:3] for item in month_list]
    month_numbers = {}
    for index, name in enumerate(month_list):
        name = name.lower()
        month_numbers[name[:3]] = index + 1
        month_numbers[name] = index + 1
    # Use the same list to create regexps for months.
    MONTH_SHORT = "(?:" + "|".join(item[:3] for item in month_list) + ")"
    MONTH_LONG = "(?:" + "|".join(item for item in month_list) + ")"

    # Same drill with weekdays, for the same reason.
    weekday_list = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                    "Saturday", "Sunday"]
    weekday_abbr_list = [item[:3] for item in weekday_list]
    WEEKDAY_SHORT = "(?:" + "|".join(item[:3] for item in weekday_list) + ")"
    WEEKDAY_LONG = "(?:" + "|".join(item for item in weekday_list) + ")"

    # This regexp tries to exclude obvious nonsense in the first pass.
    DAY_OF_MONTH = "(?:[0 ]?[1-9]|[12][0-9]|[3][01])(?!\d)"

    # Here is the overall date format; ~99% of cases fold into one generalized
    # syntax like RFC 1123, and many of the rest use asctime-like formats.
    # (see test_date_formats for a full exegesis)
    DATE = """(?ix) # Case-insensitive mode, verbose mode
        (?:
            (?P<weekday>(?:{wdy}|{weekday}),[ ])?
            (?P<day>{day})
            [ \-]
            (?P<month>{mon}|{month})
            [ \-]
            # This does not support 3-digit years, which are rare and don't
            # seem to have one canonical interpretation.
            (?P<year>(?:\d{{2}}|\d{{4}}))
            [ ]
            # HH:MM[:SS] GMT
            (?P<hour>(?:[ 0][0-9]|[01][0-9]|2[0-3]))
            :(?P<minute>(?:0[0-9]|[1-5][0-9]))
            (?::(?P<second>\d{{2}}))?
            [ ]GMT
        |
            # Support asctime format, e.g. 'Sun Nov  6 08:49:37 1994'
            (?P<weekday2>{wdy})[ ]
            (?P<month2>{mon})[ ]
            (?P<day2>[ ]\d|\d\d)[ ]
            (?P<hour2>\d\d):
            (?P<minute2>\d\d)
            (?::(?P<second2>\d\d)?)[ ]
            (?P<year2>\d\d\d\d)
            (?:[ ]GMT)?  # GMT (Amazon)
        )
    """
    DATE = DATE.format(wdy=WEEKDAY_SHORT, weekday=WEEKDAY_LONG,
                       day=DAY_OF_MONTH, mon=MONTH_SHORT, month=MONTH_LONG)

    EXPIRES_AV = "Expires=(?P<expires>%s)" % DATE

    # Now we're ready to define a regexp which can match any number of attrs
    # in the variable portion of the Set-Cookie header (like the unnamed latter
    # part of set-cookie-string in the grammar). Each regexp of any complexity
    # is split out for testing by itself.
    ATTR = """(?ix)  # Case-insensitive mode, verbose mode
        # Always start with start or semicolon and any number of spaces
        (?:^|;)[ ]*(?:
            # Big disjunction of attribute patterns (*_AV), with named capture
            # groups to extract everything in one pass. Anything unrecognized
            # goes in the 'unrecognized' capture group for reporting.
            {expires}
            |{max_age}
            |{domain}
            |{path}
            |(?P<secure>Secure=?)
            |(?P<httponly>HttpOnly=?)
            |Version=(?P<version>[{stuff}]+)
            |Comment=(?P<comment>[{stuff}]+)
            |(?P<unrecognized>[{stuff}]+)
        )
        # End with any number of spaces not matched by the preceding (up to the
        # next semicolon) - but do not capture these.
        [ ]*
    """.format(expires=EXPIRES_AV, max_age=MAX_AGE_AV, domain=DOMAIN_AV,
               path=PATH_AV, stuff=EXTENSION_AV)

    # For request data ("Cookie: ") parsing, with finditer cf. RFC 6265 4.2.1
    COOKIE = """(?x) # Verbose mode
        (?: # Either something close to valid...

            # Match starts at start of string, or at separator.
            # Split on comma for the sake of legacy code (RFC 2109/2965),
            # and since it only breaks when invalid commas are put in values.
            # see http://bugs.python.org/issue1210326
            (?:^Cookie:|^|;|,)

            # 1 or more valid token characters making up the name (captured)
            # with colon added to accommodate users of some old Java apps, etc.
            [ ]*
            (?P<name>[{name}:]+)
            [ ]*
            =
            [ ]*

            # While 6265 provides only for cookie-octet, this allows just about
            # anything in quotes (like in RFC 2616); people stuck on RFC
            # 2109/2965 will expect it to work this way. The non-quoted token
            # allows interior spaces ('\x20'), which is not valid. In both
            # cases, the decision of whether to allow these is downstream.
            (?P<value>
                ["][^\00-\31"]*["]
                |
                [{value}]
                |
                [{value}][{value} ]*[{value}]+
                |
                )

        # ... Or something way off-spec - extract to report and move on
        |
            (?P<invalid>[^;]+)
        )
        # Trailing spaces after value
        [ ]*
        # Must end with ; or be at end of string (don't consume this though,
        # so use the lookahead assertion ?=
        (?=;|\Z)
    """.format(name=COOKIE_NAME, value=COOKIE_OCTET)

    # Precompile externally useful definitions into re objects.
    COOKIE_NAME_RE = re.compile("^([%s:]+)\Z" % COOKIE_NAME)
    COOKIE_RE = re.compile(COOKIE)
    SET_COOKIE_HEADER_RE = re.compile(SET_COOKIE_HEADER)
    ATTR_RE = re.compile(ATTR)
    DATE_RE = re.compile(DATE)
    DOMAIN_RE = re.compile(DOMAIN)
    PATH_RE = re.compile('^([%s]+)\Z' % EXTENSION_AV)
    EOL = re.compile("(?:\r\n|\n)")


def strip_spaces_and_quotes(value):
    """Remove invalid whitespace and/or single pair of dquotes and return None
    for empty strings.

    Used to prepare cookie values, path, and domain attributes in a way which
    tolerates simple formatting mistakes and standards variations.
    """
    value = value.strip() if value else ""
    if value and len(value) > 1 and (value[0] == value[-1] == '"'):
        value = value[1:-1]
    if not value:
        value = ""
    return value


def parse_string(data, unquote=default_unquote):
    """Decode URL-encoded strings to UTF-8 containing the escaped chars.
    """
    if data is None:
        return None

    # We'll soon need to unquote to recover our UTF-8 data.
    # In Python 2, unquote crashes on chars beyond ASCII. So encode functions
    # had better not include anything beyond ASCII in data.
    # In Python 3, unquote crashes on bytes objects, requiring conversion to
    # str objects (unicode) using decode().
    # But in Python 2, the same decode causes unquote to butcher the data.
    # So in that case, just leave the bytes.
    if isinstance(data, bytes):
        if sys.version_info > (3, 0, 0):  # pragma: no cover
            data = data.decode('ascii')
    # Recover URL encoded data
    unquoted = unquote(data)
    # Without this step, Python 2 may have good URL decoded *bytes*,
    # which will therefore not normalize as unicode and not compare to
    # the original.
    if isinstance(unquoted, bytes):
        unquoted = unquoted.decode('utf-8')
    return unquoted


def parse_date(value):
    """Parse an RFC 1123 or asctime-like format date string to produce
    a Python datetime object (without a timezone).
    """
    # Do the regex magic; also enforces 2 or 4 digit years
    match = Definitions.DATE_RE.match(value) if value else None
    if not match:
        return None
    # We're going to extract and prepare captured data in 'data'.
    data = {}
    captured = match.groupdict()
    fields = ['year', 'month', 'day', 'hour', 'minute', 'second']
    # If we matched on the RFC 1123 family format
    if captured['year']:
        for field in fields:
            data[field] = captured[field]
    # If we matched on the asctime format, use year2 etc.
    else:
        for field in fields:
            data[field] = captured[field + "2"]
    year = data['year']
    # Interpret lame 2-digit years - base the cutoff on UNIX epoch, in case
    # someone sets a '70' cookie meaning 'distant past'. This won't break for
    # 58 years and people who use 2-digit years are asking for it anyway.
    if len(year) == 2:
        if int(year) < 70:
            year = "20" + year
        else:
            year = "19" + year
    year = int(year)
    # Clamp to [1900, 9999]: strftime has min 1900, datetime has max 9999
    data['year'] = max(1900, min(year, 9999))
    # Other things which are numbers should convert to integer
    for field in ['day', 'hour', 'minute', 'second']:
        if data[field] is None:
            data[field] = 0
        data[field] = int(data[field])
    # Look up the number datetime needs for the named month
    data['month'] = Definitions.month_numbers[data['month'].lower()]
    return datetime.datetime(**data)


def parse_domain(value):
    """Parse and validate an incoming Domain attribute value.
    """
    value = strip_spaces_and_quotes(value)
    if value:
        assert valid_domain(value)
    return value


def parse_path(value):
    """Parse and validate an incoming Path attribute value.
    """
    value = strip_spaces_and_quotes(value)
    assert valid_path(value)
    return value


def parse_value(value, allow_spaces=True, unquote=default_unquote):
    "Process a cookie value"
    if value is None:
        return None
    value = strip_spaces_and_quotes(value)
    value = parse_string(value, unquote=unquote)
    if not allow_spaces:
        assert ' ' not in value
    return value


def valid_name(name):
    "Validate a cookie name string"
    if isinstance(name, bytes):
        name = name.decode('ascii')
    if not Definitions.COOKIE_NAME_RE.match(name):
        return False
    # This module doesn't support $identifiers, which are part of an obsolete
    # and highly complex standard which is never used.
    if name[0] == "$":
        return False
    return True


def valid_value(value, quote=default_cookie_quote, unquote=default_unquote):
    """Validate a cookie value string.

    This is generic across quote/unquote functions because it directly verifies
    the encoding round-trip using the specified quote/unquote functions.
    So if you use different quote/unquote functions, use something like this
    as a replacement for valid_value::

        my_valid_value = lambda value: valid_value(value, quote=my_quote,
                                                          unquote=my_unquote)
    """
    if value is None:
        return False

    # Put the value through a round trip with the given quote and unquote
    # functions, so we will know whether data will get lost or not in the event
    # that we don't complain.
    encoded = encode_cookie_value(value, quote=quote)
    decoded = parse_string(encoded, unquote=unquote)

    # If the original string made the round trip, this is a valid value for the
    # given quote and unquote functions. Since the round trip can generate
    # different unicode forms, normalize before comparing, so we can ignore
    # trivial inequalities.
    decoded_normalized = (normalize("NFKD", decoded)
                          if not isinstance(decoded, bytes) else decoded)
    value_normalized = (normalize("NFKD", value)
                        if not isinstance(value, bytes) else value)
    if decoded_normalized == value_normalized:
        return True
    return False


def valid_date(date):
    "Validate an expires datetime object"
    # We want something that acts like a datetime. In particular,
    # strings indicate a failure to parse down to an object and ints are
    # nonstandard and ambiguous at best.
    if not hasattr(date, 'tzinfo'):
        return False
    # Relevant RFCs define UTC as 'close enough' to GMT, and the maximum
    # difference between UTC and GMT is often stated to be less than a second.
    if date.tzinfo is None or _total_seconds(date.utcoffset()) < 1.1:
        return True
    return False


def valid_domain(domain):
    "Validate a cookie domain ASCII string"
    # Using encoding on domain would confuse browsers into not sending cookies.
    # Generate UnicodeDecodeError up front if it can't store as ASCII.
    domain.encode('ascii')
    # Domains starting with periods are not RFC-valid, but this is very common
    # in existing cookies, so they should still parse with DOMAIN_AV.
    if Definitions.DOMAIN_RE.match(domain):
        return True
    return False


def valid_path(value):
    "Validate a cookie path ASCII string"
    # Generate UnicodeDecodeError if path can't store as ASCII.
    value.encode("ascii")
    # Cookies without leading slash will likely be ignored, raise ASAP.
    if not (value and value[0] == "/"):
        return False
    if not Definitions.PATH_RE.match(value):
        return False
    return True


def valid_max_age(number):
    "Validate a cookie Max-Age"
    if isinstance(number, basestring):
        try:
            number = long(number)
        except (ValueError, TypeError):
            return False
    if number >= 0 and number % 1 == 0:
        return True
    return False


def encode_cookie_value(data, quote=default_cookie_quote):
    """URL-encode strings to make them safe for a cookie value.

    By default this uses urllib quoting, as used in many other cookie
    implementations and in other Python code, instead of an ad hoc escaping
    mechanism which includes backslashes (these also being illegal chars in RFC
    6265).
    """
    if data is None:
        return None

    # encode() to ASCII bytes so quote won't crash on non-ASCII.
    # but doing that to bytes objects is nonsense.
    # On Python 2 encode crashes if s is bytes containing non-ASCII.
    # On Python 3 encode crashes on all byte objects.
    if not isinstance(data, bytes):
        data = data.encode("utf-8")

    # URL encode data so it is safe for cookie value
    quoted = quote(data)

    # Don't force to bytes, so that downstream can use proper string API rather
    # than crippled bytes, and to encourage encoding to be done just once.
    return quoted


def encode_extension_av(data, quote=default_extension_quote):
    """URL-encode strings to make them safe for an extension-av
    (extension attribute value): <any CHAR except CTLs or ";">
    """
    if not data:
        return ''
    return quote(data)


def render_date(date):
    """Render a date (e.g. an Expires value) per RFCs 6265/2616/1123.

    Don't give this localized (timezone-aware) datetimes. If you use them,
    convert them to GMT before passing them to this. There are too many
    conversion corner cases to handle this universally.
    """
    if not date:
        return None
    assert valid_date(date)
    # Avoid %a and %b, which can change with locale, breaking compliance
    weekday = Definitions.weekday_abbr_list[date.weekday()]
    month = Definitions.month_abbr_list[date.month - 1]
    return date.strftime("{day}, %d {month} %Y %H:%M:%S GMT"
                         ).format(day=weekday, month=month)


def render_domain(domain):
    if not domain:
        return None
    if domain[0] == '.':
        return domain[1:]
    return domain


def _parse_request(header_data, ignore_bad_cookies=False):
    """Turn one or more lines of 'Cookie:' header data into a dict mapping
    cookie names to cookie values (raw strings).
    """
    cookies_dict = {}
    for line in Definitions.EOL.split(header_data.strip()):
        matches = Definitions.COOKIE_RE.finditer(line)
        matches = [item for item in matches]
        for match in matches:
            invalid = match.group('invalid')
            if invalid:
                if not ignore_bad_cookies:
                    raise InvalidCookieError(data=invalid)
                _report_invalid_cookie(invalid)
                continue
            name = match.group('name')
            values = cookies_dict.get(name)
            value = match.group('value').strip('"')
            if values:
                values.append(value)
            else:
                cookies_dict[name] = [value]
        if not matches:
            if not ignore_bad_cookies:
                raise InvalidCookieError(data=line)
            _report_invalid_cookie(line)
    return cookies_dict


def parse_one_response(line, ignore_bad_cookies=False,
                       ignore_bad_attributes=True):
    """Turn one 'Set-Cookie:' line into a dict mapping attribute names to
    attribute values (raw strings).
    """
    cookie_dict = {}
    # Basic validation, extract name/value/attrs-chunk
    match = Definitions.SET_COOKIE_HEADER_RE.match(line)
    if not match:
        if not ignore_bad_cookies:
            raise InvalidCookieError(data=line)
        _report_invalid_cookie(line)
        return None
    cookie_dict.update({
        'name': match.group('name'),
        'value': match.group('value')})
    # Extract individual attrs from the attrs chunk
    for match in Definitions.ATTR_RE.finditer(match.group('attrs')):
        captured = dict((k, v) for (k, v) in match.groupdict().items() if v)
        unrecognized = captured.get('unrecognized', None)
        if unrecognized:
            if not ignore_bad_attributes:
                raise InvalidCookieAttributeError(None, unrecognized,
                                                  "unrecognized")
            _report_unknown_attribute(unrecognized)
            continue
        # for unary flags
        for key in ('secure', 'httponly'):
            if captured.get(key):
                captured[key] = True
        # ignore subcomponents of expires - they're still there to avoid doing
        # two passes
        timekeys = ('weekday', 'month', 'day', 'hour', 'minute', 'second',
                    'year')
        if 'year' in captured:
            for key in timekeys:
                del captured[key]
        elif 'year2' in captured:
            for key in timekeys:
                del captured[key + "2"]
        cookie_dict.update(captured)
    return cookie_dict


def _parse_response(header_data, ignore_bad_cookies=False,
                    ignore_bad_attributes=True):
    """Turn one or more lines of 'Set-Cookie:' header data into a list of dicts
    mapping attribute names to attribute values (as plain strings).
    """
    cookie_dicts = []
    for line in Definitions.EOL.split(header_data.strip()):
        if not line:
            break
        cookie_dict = parse_one_response(
            line, ignore_bad_cookies=ignore_bad_cookies,
            ignore_bad_attributes=ignore_bad_attributes)
        if not cookie_dict:
            continue
        cookie_dicts.append(cookie_dict)
    if not cookie_dicts:
        if not ignore_bad_cookies:
            raise InvalidCookieError(data=header_data)
        _report_invalid_cookie(header_data)
    return cookie_dicts


class Cookie(object):
    """Provide a simple interface for creating, modifying, and rendering
    individual HTTP cookies.

    Cookie attributes are represented as normal Python object attributes.
    Parsing, rendering and validation are reconfigurable per-attribute. The
    default behavior is intended to comply with RFC 6265, URL-encoding illegal
    characters where necessary. For example: the default behavior for the
    Expires attribute is to parse strings as datetimes using parse_date,
    validate that any set value is a datetime, and render the attribute per the
    preferred date format in RFC 1123.
    """
    def __init__(self, name, value, **kwargs):
        # If we don't have or can't set a name value, we don't want to return
        # junk, so we must break control flow. And we don't want to use
        # InvalidCookieAttributeError, because users may want to catch that to
        # suppress all complaining about funky attributes.
        try:
            self.name = name
        except InvalidCookieAttributeError:
            raise InvalidCookieError(message="invalid name for new Cookie",
                                     data=name)
        value = value or ''
        try:
            self.value = value
        except InvalidCookieAttributeError:
            raise InvalidCookieError(message="invalid value for new Cookie",
                                     data=value)
        if kwargs:
            self._set_attributes(kwargs, ignore_bad_attributes=False)

    def _set_attributes(self, attrs, ignore_bad_attributes=False):
        for attr_name, attr_value in attrs.items():
            if not attr_name in self.attribute_names:
                if not ignore_bad_attributes:
                    raise InvalidCookieAttributeError(
                        attr_name, attr_value,
                        "unknown cookie attribute '%s'" % attr_name)
                _report_unknown_attribute(attr_name)

            try:
                setattr(self, attr_name, attr_value)
            except InvalidCookieAttributeError as error:
                if not ignore_bad_attributes:
                    raise
                _report_invalid_attribute(attr_name, attr_value, error.reason)
                continue

    @classmethod
    def from_dict(cls, cookie_dict, ignore_bad_attributes=True):
        """Construct an instance from a dict of strings to parse.

        The main difference between this and Cookie(name, value, **kwargs) is
        that the values in the argument to this method are parsed.

        If ignore_bad_attributes=True (default), values which did not parse
        are set to '' in order to avoid passing bad data.
        """
        name = cookie_dict.get('name', None)
        if not name:
            raise InvalidCookieError("Cookie must have name")
        raw_value = cookie_dict.get('value', '')
        # Absence or failure of parser here is fatal; errors in present name
        # and value should be found by Cookie.__init__.
        value = cls.attribute_parsers['value'](raw_value)
        cookie = cls(name, value)

        # Parse values from serialized formats into objects
        parsed = {}
        for key, value in cookie_dict.items():
            # Don't want to pass name/value to _set_attributes
            if key in ('name', 'value'):
                continue
            parser = cls.attribute_parsers.get(key)
            if not parser:
                # Don't let totally unknown attributes pass silently
                if not ignore_bad_attributes:
                    raise InvalidCookieAttributeError(
                        key, value, "unknown cookie attribute '%s'" % key)
                _report_unknown_attribute(key)
                continue
            try:
                parsed_value = parser(value)
            except Exception as e:
                reason = "did not parse with %r: %r" % (parser, e)
                if not ignore_bad_attributes:
                    raise InvalidCookieAttributeError(
                        key, value, reason)
                _report_invalid_attribute(key, value, reason)
                parsed_value = ''
            parsed[key] = parsed_value

        # Set the parsed objects (does object validation automatically)
        cookie._set_attributes(parsed, ignore_bad_attributes)
        return cookie

    @classmethod
    def from_string(cls, line, ignore_bad_cookies=False,
                    ignore_bad_attributes=True):
        "Construct a Cookie object from a line of Set-Cookie header data."
        cookie_dict = parse_one_response(
            line, ignore_bad_cookies=ignore_bad_cookies,
            ignore_bad_attributes=ignore_bad_attributes)
        if not cookie_dict:
            return None
        return cls.from_dict(
            cookie_dict, ignore_bad_attributes=ignore_bad_attributes)

    def to_dict(self):
        this_dict = {'name': self.name, 'value': self.value}
        this_dict.update(self.attributes())
        return this_dict

    def validate(self, name, value):
        """Validate a cookie attribute with an appropriate validator.

        The value comes in already parsed (for example, an expires value
        should be a datetime). Called automatically when an attribute
        value is set.
        """
        validator = self.attribute_validators.get(name, None)
        if validator:
            return True if validator(value) else False
        return True

    def __setattr__(self, name, value):
        """Attributes mentioned in attribute_names get validated using
        functions in attribute_validators, raising an exception on failure.
        Others get left alone.
        """
        if name in self.attribute_names or name in ("name", "value"):
            if name == 'name' and not value:
                raise InvalidCookieError(message="Cookies must have names")
            # Ignore None values indicating unset attr. Other invalids should
            # raise error so users of __setattr__ can learn.
            if value is not None:
                if not self.validate(name, value):
                    raise InvalidCookieAttributeError(
                        name, value, "did not validate with " +
                        repr(self.attribute_validators.get(name)))
        object.__setattr__(self, name, value)

    def __getattr__(self, name):
        """Provide for acting like everything in attribute_names is
        automatically set to None, rather than having to do so explicitly and
        only at import time.
        """
        if name in self.attribute_names:
            return None
        raise AttributeError(name)

    def attributes(self):
        """Export this cookie's attributes as a dict of encoded values.

        This is an important part of the code for rendering attributes, e.g.
        render_response().
        """
        dictionary = {}
        # Only look for attributes registered in attribute_names.
        for python_attr_name, cookie_attr_name in self.attribute_names.items():
            value = getattr(self, python_attr_name)
            renderer = self.attribute_renderers.get(python_attr_name, None)
            if renderer:
                value = renderer(value)
            # If renderer returns None, or it's just natively none, then the
            # value is suppressed entirely - does not appear in any rendering.
            if not value:
                continue
            dictionary[cookie_attr_name] = value
        return dictionary

    def render_request(self):
        """Render as a string formatted for HTTP request headers
        (simple 'Cookie: ' style).
        """
        # Use whatever renderers are defined for name and value.
        name, value = self.name, self.value
        renderer = self.attribute_renderers.get('name', None)
        if renderer:
            name = renderer(name)
        renderer = self.attribute_renderers.get('value', None)
        if renderer:
            value = renderer(value)
        return ''.join((name, "=", value))

    def render_response(self):
        """Render as a string formatted for HTTP response headers
        (detailed 'Set-Cookie: ' style).
        """
        # Use whatever renderers are defined for name and value.
        # (.attributes() is responsible for all other rendering.)
        name, value = self.name, self.value
        renderer = self.attribute_renderers.get('name', None)
        if renderer:
            name = renderer(name)
        renderer = self.attribute_renderers.get('value', None)
        if renderer:
            value = renderer(value)
        return '; '.join(
            ['{0}={1}'.format(name, value)] +
            [key if isinstance(val, bool) else '='.join((key, val))
             for key, val in self.attributes().items()]
        )

    def __eq__(self, other):
        attrs = ['name', 'value'] + list(self.attribute_names.keys())
        for attr in attrs:
            mine = getattr(self, attr, None)
            his = getattr(other, attr, None)
            if isinstance(mine, bytes):
                mine = mine.decode('utf-8')
            if isinstance(his, bytes):
                his = his.decode('utf-8')
            if attr == 'domain':
                if mine and mine[0] == '.':
                    mine = mine[1:]
                if his and his[0] == '.':
                    his = his[1:]
            if mine != his:
                return False
        return True

    def __ne__(self, other):
        return not self.__eq__(other)

    # Add a name and its proper rendering to this dict to register an attribute
    # as exportable. The key is the name of the Cookie object attribute in
    # Python, and it is mapped to the name you want in the output.
    # 'name' and 'value' should not be here.
    attribute_names = {
        'expires':  'Expires',
        'max_age':  'Max-Age',
        'domain':   'Domain',
        'path':     'Path',
        'comment':  'Comment',
        'version':  'Version',
        'secure':   'Secure',
        'httponly': 'HttpOnly',
    }

    # Register single-parameter functions in this dictionary to have them
    # used for encoding outgoing values (e.g. as RFC compliant strings,
    # as base64, encrypted stuff, etc.)
    # These are called by the property generated by cookie_attribute().
    # Usually it would be wise not to define a renderer for name, but it is
    # supported in case there is ever a real need.
    attribute_renderers = {
        'value':    encode_cookie_value,
        'domain':   render_domain,
        'expires':  render_date,
        'max_age':  lambda item: str(item) if item is not None else None,
        'secure':   lambda item: True if item else False,
        'httponly': lambda item: True if item else False,
        'comment':  encode_extension_av,
        'version':  lambda item: (str(item) if isinstance(item, int)
                                  else encode_extension_av(item)),
    }

    # Register single-parameter functions in this dictionary to have them used
    # for decoding incoming values for use in the Python API (e.g. into nice
    # objects, numbers, unicode strings, etc.)
    # These are called by the property generated by cookie_attribute().
    attribute_parsers = {
        'value':    parse_value,
        'expires':  parse_date,
        'domain':   parse_domain,
        'path':     parse_path,
        'max_age':  lambda item: long(strip_spaces_and_quotes(item)),
        'comment':  parse_string,
        'version':  lambda item: int(strip_spaces_and_quotes(item)),
        'secure':   lambda item: True if item else False,
        'httponly': lambda item: True if item else False,
    }

    # Register single-parameter functions which return a true value for
    # acceptable values, and a false value for unacceptable ones. An
    # attribute's validator is run after it is parsed or when it is directly
    # set, and InvalidCookieAttribute is raised if validation fails (and the
    # validator doesn't raise a different exception prior)
    attribute_validators = {
        'name':     valid_name,
        'value':    valid_value,
        'expires':  valid_date,
        'domain':   valid_domain,
        'path':     valid_path,
        'max_age':  valid_max_age,
        'comment':  valid_value,
        'version':  lambda number: re.match("^\d+\Z", str(number)),
        'secure':   lambda item: item is True or item is False,
        'httponly': lambda item: item is True or item is False,
    }


class Cookies(dict):
    """Represent a set of cookies indexed by name.

    This class bundles together a set of Cookie objects and provides
    a convenient interface to them. for parsing and producing cookie headers.
    In basic operation it acts just like a dict of Cookie objects, but it adds
    additional convenience methods for the usual cookie tasks: add cookie
    objects by their names, create new cookie objects under specified names,
    parse HTTP request or response data into new cookie objects automatically
    stored in the dict, and render the set in formats suitable for HTTP request
    or response headers.
    """
    DEFAULT_COOKIE_CLASS = Cookie

    def __init__(self, *args, **kwargs):
        dict.__init__(self)
        self.all_cookies = []
        self.cookie_class = kwargs.get(
            "_cookie_class", self.DEFAULT_COOKIE_CLASS)
        self.add(*args, **kwargs)

    def add(self, *args, **kwargs):
        """Add Cookie objects by their names, or create new ones under
        specified names.

        Any unnamed arguments are interpreted as existing cookies, and
        are added under the value in their .name attribute. With keyword
        arguments, the key is interpreted as the cookie name and the
        value as the UNENCODED value stored in the cookie.
        """
        # Only the first one is accessible through the main interface,
        # others accessible through get_all (all_cookies).
        for cookie in args:
            self.all_cookies.append(cookie)
            if cookie.name in self:
                continue
            self[cookie.name] = cookie
        for key, value in kwargs.items():
            cookie = self.cookie_class(key, value)
            self.all_cookies.append(cookie)
            if key in self:
                continue
            self[key] = cookie

    def get_all(self, key):
        return [cookie for cookie in self.all_cookies
                if cookie.name == key]

    def parse_request(self, header_data, ignore_bad_cookies=False):
        """Parse 'Cookie' header data into Cookie objects, and add them to
        this Cookies object.

        :arg header_data: string containing only 'Cookie:' request headers or
        header values (as in CGI/WSGI HTTP_COOKIE); if more than one, they must
        be separated by CRLF (\\r\\n).

        :arg ignore_bad_cookies: if set, will log each syntactically invalid
        cookie (at the granularity of semicolon-delimited blocks) rather than
        raising an exception at the first bad cookie.

        :returns: a Cookies instance containing Cookie objects parsed from
        header_data.

        .. note::
        If you want to parse 'Set-Cookie:' response headers, please use
        parse_response instead. parse_request will happily turn 'expires=frob'
        into a separate cookie without complaining, according to the grammar.
        """
        cookies_dict = _parse_request(
            header_data, ignore_bad_cookies=ignore_bad_cookies)
        cookie_objects = []
        for name, values in cookies_dict.items():
            for value in values:
                # Use from_dict to check name and parse value
                cookie_dict = {'name': name, 'value': value}
                try:
                    cookie = self.cookie_class.from_dict(cookie_dict)
                except InvalidCookieError:
                    if not ignore_bad_cookies:
                        raise
                else:
                    cookie_objects.append(cookie)
        try:
            self.add(*cookie_objects)
        except InvalidCookieError:
            if not ignore_bad_cookies:
                raise
            _report_invalid_cookie(header_data)
        return self

    def parse_response(self, header_data, ignore_bad_cookies=False,
                       ignore_bad_attributes=True):
        """Parse 'Set-Cookie' header data into Cookie objects, and add them to
        this Cookies object.

        :arg header_data: string containing only 'Set-Cookie:' request headers
        or their corresponding header values; if more than one, they must be
        separated by CRLF (\\r\\n).

        :arg ignore_bad_cookies: if set, will log each syntactically invalid
        cookie rather than raising an exception at the first bad cookie. (This
        includes cookies which have noncompliant characters in the attribute
        section).

        :arg ignore_bad_attributes: defaults to True, which means to log but
        not raise an error when a particular attribute is unrecognized. (This
        does not necessarily mean that the attribute is invalid, although that
        would often be the case.) if unset, then an error will be raised at the
        first semicolon-delimited block which has an unknown attribute.

        :returns: a Cookies instance containing Cookie objects parsed from
        header_data, each with recognized attributes populated.

        .. note::
        If you want to parse 'Cookie:' headers (i.e., data like what's sent
        with an HTTP request, which has only name=value pairs and no
        attributes), then please use parse_request instead. Such lines often
        contain multiple name=value pairs, and parse_response will throw away
        the pairs after the first one, which will probably generate errors or
        confusing behavior. (Since there's no perfect way to automatically
        determine which kind of parsing to do, you have to tell it manually by
        choosing correctly from parse_request between part_response.)
        """
        cookie_dicts = _parse_response(
            header_data,
            ignore_bad_cookies=ignore_bad_cookies,
            ignore_bad_attributes=ignore_bad_attributes)
        cookie_objects = []
        for cookie_dict in cookie_dicts:
            cookie = self.cookie_class.from_dict(cookie_dict)
            cookie_objects.append(cookie)
        self.add(*cookie_objects)
        return self

    @classmethod
    def from_request(cls, header_data, ignore_bad_cookies=False):
        "Construct a Cookies object from request header data."
        cookies = cls()
        cookies.parse_request(
            header_data, ignore_bad_cookies=ignore_bad_cookies)
        return cookies

    @classmethod
    def from_response(cls, header_data, ignore_bad_cookies=False,
                      ignore_bad_attributes=True):
        "Construct a Cookies object from response header data."
        cookies = cls()
        cookies.parse_response(
            header_data,
            ignore_bad_cookies=ignore_bad_cookies,
            ignore_bad_attributes=ignore_bad_attributes)
        return cookies

    def render_request(self, sort=True):
        """Render the dict's Cookie objects into a string formatted for HTTP
        request headers (simple 'Cookie: ' style).
        """
        if not sort:
            return ("; ".join(
                cookie.render_request() for cookie in self.values()))
        return ("; ".join(sorted(
            cookie.render_request() for cookie in self.values())))

    def render_response(self, sort=True):
        """Render the dict's Cookie objects into list of strings formatted for
        HTTP response headers (detailed 'Set-Cookie: ' style).
        """
        rendered = [cookie.render_response() for cookie in self.values()]
        return rendered if not sort else sorted(rendered)

    def __repr__(self):
        return "Cookies(%s)" % ', '.join("%s=%r" % (name, cookie.value) for
                                         (name, cookie) in self.items())

    def __eq__(self, other):
        """Test if a Cookies object is globally 'equal' to another one by
        seeing if it looks like a dict such that d[k] == self[k]. This depends
        on each Cookie object reporting its equality correctly.
        """
        if not hasattr(other, "keys"):
            return False
        try:
            keys = sorted(set(self.keys()) | set(other.keys()))
            for key in keys:
                if not key in self:
                    return False
                if not key in other:
                    return False
                if self[key] != other[key]:
                    return False
        except (TypeError, KeyError):
            raise
        return True

    def __ne__(self, other):
        return not self.__eq__(other)
