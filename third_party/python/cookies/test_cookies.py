#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Tests for code in cookies.py.
"""
from __future__ import unicode_literals
import re
import sys
import logging
if sys.version_info < (3, 0, 0):
    from urllib import quote, unquote
else:
    from urllib.parse import quote, unquote
    unichr = chr
    basestring = str
from datetime import datetime, tzinfo, timedelta
from pytest import raises

from cookies import (
        InvalidCookieError, InvalidCookieAttributeError,
        Definitions,
        Cookie, Cookies,
        render_date, parse_date,
        parse_string, parse_value, parse_domain, parse_path,
        parse_one_response,
        encode_cookie_value, encode_extension_av,
        valid_value, valid_date, valid_domain, valid_path,
        strip_spaces_and_quotes, _total_seconds,
        )


class RFC1034:
    """Definitions from RFC 1034: 'DOMAIN NAMES - CONCEPTS AND FACILITIES'
    section 3.5, as cited in RFC 6265 4.1.1.
    """
    digit = "[0-9]"
    letter = "[A-Za-z]"
    let_dig = "[0-9A-Za-z]"
    let_dig_hyp = "[0-9A-Za-z\-]"
    assert "\\" in let_dig_hyp
    ldh_str = "%s+" % let_dig_hyp
    label = "(?:%s|%s|%s)" % (
            letter,
            letter + let_dig,
            letter + ldh_str + let_dig)
    subdomain = "(?:%s\.)*(?:%s)" % (label, label)
    domain = "( |%s)" % (subdomain)

    def test_sanity(self):
        "Basic smoke tests that definitions transcribed OK"
        match = re.compile("^%s\Z" % self.domain).match
        assert match("A.ISI.EDU")
        assert match("XX.LCS.MIT.EDU")
        assert match("SRI-NIC.ARPA")
        assert not match("foo+bar")
        assert match("foo.com")
        assert match("foo9.com")
        assert not match("9foo.com")
        assert not match("26.0.0.73.COM")
        assert not match(".woo.com")
        assert not match("blop.foo.")
        assert match("foo-bar.com")
        assert not match("-foo.com")
        assert not match("foo.com-")


class RFC1123:
    """Definitions from RFC 1123: "Requirements for Internet Hosts --
    Application and Support" section 2.1, cited in RFC 6265 section
    4.1.1 as an update to RFC 1034.
    Here this is really just used for testing Domain attribute values.
    """
    # Changed per 2.1 (similar to some changes in RFC 1101)
    # this implementation is a bit simpler...
    # n.b.: there are length limits in the real thing
    label = "{let_dig}(?:(?:{let_dig_hyp}+)?{let_dig})?".format(
            let_dig=RFC1034.let_dig, let_dig_hyp=RFC1034.let_dig_hyp)
    subdomain = "(?:%s\.)*(?:%s)" % (label, label)
    domain = "( |%s)" % (subdomain)

    def test_sanity(self):
        "Basic smoke tests that definitions transcribed OK"
        match = re.compile("^%s\Z" % self.domain).match
        assert match("A.ISI.EDU")
        assert match("XX.LCS.MIT.EDU")
        assert match("SRI-NIC.ARPA")
        assert not match("foo+bar")
        assert match("foo.com")
        assert match("9foo.com")
        assert match("3Com.COM")
        assert match("3M.COM")


class RFC2616:
    """Definitions from RFC 2616 section 2.2, as cited in RFC 6265 4.1.1
    """
    SEPARATORS = '()<>@,;:\\"/[]?={} \t'


class RFC5234:
    """Basic definitions per RFC 5234: 'Augmented BNF for Syntax
    Specifications'
    """
    CHAR = "".join([chr(i) for i in range(0, 127 + 1)])
    CTL = "".join([chr(i) for i in range(0, 31 + 1)]) + "\x7f"
    # this isn't in the RFC but it can be handy
    NONCTL = "".join([chr(i) for i in range(32, 127)])
    # this is what the RFC says about a token more or less verbatim
    TOKEN = "".join(sorted(set(NONCTL) - set(RFC2616.SEPARATORS)))


class FixedOffsetTz(tzinfo):
    """A tzinfo subclass for attaching to datetime objects.

    Used for various tests involving date parsing, since Python stdlib does not
    obviously provide tzinfo subclasses and testing this module only requires
    a very simple one.
    """
    def __init__(self, offset):
        # tzinfo.utcoffset() throws an error for sub-minute amounts,
        # so round
        minutes = round(offset / 60.0, 0)
        self.__offset = timedelta(minutes=minutes)

    def utcoffset(self, dt):
        return self.__offset

    def tzname(self, dt):
        return "FixedOffsetTz" + str(self.__offset.seconds)

    def dst(self, dt):
        return timedelta(0)


class TestInvalidCookieError(object):
    """Exercise the trivial behavior of the InvalidCookieError exception.
    """
    def test_simple(self):
        "This be the test"
        def exception(data):
            "Gather an InvalidCookieError exception"
            try:
                raise InvalidCookieError(data)
            except InvalidCookieError as exception:
                return exception
            # other exceptions will pass through
            return None
        assert exception("no donut").data == "no donut"

        # Spot check for obvious junk in loggable representations.
        e = exception("yay\x00whee")
        assert "\x00" not in repr(e)
        assert "\x00" not in str(e)
        assert "yaywhee" not in repr(e)
        assert "yaywhee" not in str(e)
        assert "\n" not in repr(exception("foo\nbar"))


class TestInvalidCookieAttributeError(object):
    """Exercise the trivial behavior of InvalidCookieAttributeError.
    """
    def exception(self, *args, **kwargs):
        "Generate an InvalidCookieAttributeError exception naturally"
        try:
            raise InvalidCookieAttributeError(*args, **kwargs)
        except InvalidCookieAttributeError as exception:
            return exception
        return None

    def test_simple(self):
        e = self.exception("foo", "bar")
        assert e.name == "foo"
        assert e.value == "bar"

    def test_junk_in_loggables(self):
        # Spot check for obvious junk in loggable representations.
        # This isn't completely idle: for example, nulls are ignored in
        # %-formatted text, and this could be very misleading
        e = self.exception("ya\x00y", "whee")
        assert "\x00" not in repr(e)
        assert "\x00" not in str(e)
        assert "yay" not in repr(e)
        assert "yay" not in str(e)

        e = self.exception("whee", "ya\x00y")
        assert "\x00" not in repr(e)
        assert "\x00" not in str(e)
        assert "yay" not in repr(e)
        assert "yay" not in str(e)

        assert "\n" not in repr(self.exception("yay", "foo\nbar"))
        assert "\n" not in repr(self.exception("foo\nbar", "yay"))

    def test_no_name(self):
        # not recommended to do this, but we want to handle it if people do
        e = self.exception(None, "stuff")
        assert e.name == None
        assert e.value == "stuff"
        assert e.reason == None
        assert 'stuff' in str(e)


class TestDefinitions(object):
    """Test the patterns in cookies.Definitions against specs.
    """
    def test_cookie_name(self, check_unicode=False):
        """Check COOKIE_NAME against the token definition in RFC 2616 2.2 (as
        cited in RFC 6265):

       token          = 1*<any CHAR except CTLs or separators>
       separators     = "(" | ")" | "<" | ">" | "@"
                      | "," | ";" | ":" | "\" | <">
                      | "/" | "[" | "]" | "?" | "="
                      | "{" | "}" | SP | HT

        (Definitions.COOKIE_NAME is regex-ready while RFC5234.TOKEN is more
        clearly related to the RFC; they should be functionally the same)
        """
        regex = Definitions.COOKIE_NAME_RE
        assert regex.match(RFC5234.TOKEN)
        assert not regex.match(RFC5234.NONCTL)
        for c in RFC5234.CTL:
            assert not regex.match(c)
        for c in RFC2616.SEPARATORS:
            # Skip special case - some number of Java and PHP apps have used
            # colon in names, while this is dumb we want to not choke on this
            # by default since it may be the single biggest cause of bugs filed
            # against Python's cookie libraries
            if c == ':':
                continue
            assert not regex.match(c)
        # Unicode over 7 bit ASCII shouldn't match, but this takes a while
        if check_unicode:
            for i in range(127, 0x10FFFF + 1):
                assert not regex.match(unichr(i))

    def test_cookie_octet(self):
        """Check COOKIE_OCTET against the definition in RFC 6265:

        cookie-octet      = %x21 / %x23-2B / %x2D-3A / %x3C-5B / %x5D-7E
                              ; US-ASCII characters excluding CTLs,
                              ; whitespace DQUOTE, comma, semicolon,
                              ; and backslash
        """
        match = re.compile("^[%s]+\Z" % Definitions.COOKIE_OCTET).match
        for c in RFC5234.CTL:
            assert not match(c)
            assert not match("a%sb" % c)
        # suspect RFC typoed 'whitespace, DQUOTE' as 'whitespace DQUOTE'
        assert not match(' ')
        assert not match('"')
        assert not match(',')
        assert not match(';')
        assert not match('\\')
        # the spec above DOES include =.-
        assert match("=")
        assert match(".")
        assert match("-")

        # Check that everything else in CHAR works.
        safe_cookie_octet = "".join(sorted(
            set(RFC5234.NONCTL) - set(' ",;\\')))
        assert match(safe_cookie_octet)

    def test_set_cookie_header(self):
        """Smoke test SET_COOKIE_HEADER (used to compile SET_COOKIE_HEADER_RE)
        against HEADER_CASES.
        """
        # should match if expectation is not an error, shouldn't match if it is
        # an error. set-cookie-header is for responses not requests, so use
        # response expectation rather than request expectation
        match = re.compile(Definitions.SET_COOKIE_HEADER).match
        for case in HEADER_CASES:
            arg, kwargs, request_result, expected = case
            this_match = match(arg)
            if expected and not isinstance(expected, type):
                assert this_match, "should match as response: " + repr(arg)
            else:
                if not request_result:
                    assert not this_match, \
                            "should not match as response: " + repr(arg)

    def test_cookie_cases(self):
        """Smoke test COOKIE_HEADER (used to compile COOKIE_HEADER_RE) against
        HEADER_CASES.
        """
        # should match if expectation is not an error, shouldn't match if it is
        # an error. cookie-header is for requests not responses, so use request
        # expectation rather than response expectation
        match = re.compile(Definitions.COOKIE).match
        for case in HEADER_CASES:
            arg, kwargs, expected, response_result = case
            this_match = match(arg)
            if expected and not isinstance(expected, type):
                assert this_match, "should match as request: " + repr(arg)
            else:
                if not response_result:
                    assert not this_match, \
                    "should not match as request: " + repr(arg)

    def test_cookie_pattern(self):
        """Smoke test Definitions.COOKIE (used to compile COOKIE_RE) against
        the grammar for cookie-header as in RFC 6265.

         cookie-header     = "Cookie:" OWS cookie-string OWS
         cookie-string     = cookie-pair *( ";" SP cookie-pair )
         cookie-pair       = cookie-name "=" cookie-value
         cookie-name       = token
         cookie-value      = *cookie-octet / ( DQUOTE *cookie-octet DQUOTE )

         cookie-name and cookie-value are not broken apart for separate
         testing, as the former is essentially just token and the latter
         essentially just cookie-octet.
        """
        match = re.compile(Definitions.COOKIE).match
        # cookie-pair behavior around =
        assert match("foo").group('invalid')
        assert match("foo=bar")
        # Looks dumb, but this is legal because "=" is valid for cookie-octet.
        assert match("a=b=c")
        # DQUOTE *cookie-octet DQUOTE - allowed
        assert match('foo="bar"')

        # for testing on the contents of cookie name and cookie value,
        # see test_cookie_name and test_cookie_octet.

        regex = re.compile(Definitions.COOKIE)
        correct = [
            ('foo', 'yar', ''),
            ('bar', 'eeg', ''),
            ('baz', 'wog', ''),
            ('frob', 'laz', '')]

        def assert_correct(s):
            #naive = re.findall(" *([^;]+)=([^;]+) *(?:;|\Z)", s)
            result = regex.findall(s)
            assert result == correct
        # normal-looking case should work normally
        assert_correct("foo=yar; bar=eeg; baz=wog; frob=laz")
        # forgive lack of whitespace as long as semicolons are explicit
        assert_correct("foo=yar;bar=eeg;baz=wog;frob=laz")
        # forgive too much whitespace AROUND values
        assert_correct("  foo=yar;  bar=eeg;  baz=wog;   frob=laz  ")

        # Actually literal spaces are NOT allowed in cookie values per RFC 6265
        # and it is UNWISE to put them in without escaping. But we want the
        # flexibility to let this pass with a warning, because this is the kind
        # of bad idea which is very common and results in loud complaining on
        # issue trackers on the grounds that PHP does it or something. So the
        # regex is weakened, but the presence of a space should still be at
        # least noted, and an exception must be raised if = is also used
        # - because that would often indicate the loss of cookies due to
        # forgotten separator, as in "foo=yar bar=eeg baz=wog frob=laz".
        assert regex.findall("foo=yar; bar=eeg; baz=wog; frob=l az") == [
            ('foo', 'yar', ''),
            ('bar', 'eeg', ''),
            ('baz', 'wog', ''),
            # handle invalid internal whitespace.
            ('frob', 'l az', '')
            ]

        # Without semicolons or inside semicolon-delimited blocks, the part
        # before the first = should be interpreted as a name, and the rest as
        # a value (since = is not forbidden for cookie values). Thus:
        result = regex.findall("foo=yarbar=eegbaz=wogfrob=laz")
        assert result[0][0] == 'foo'
        assert result[0][1] == 'yarbar=eegbaz=wogfrob=laz'
        assert result[0][2] == ''

        # Make some bad values and see that it's handled reasonably.
        # (related to http://bugs.python.org/issue2988)
        # don't test on semicolon because the regexp stops there, reasonably.
        for c in '\x00",\\':
            nasty = "foo=yar" + c + "bar"
            result = regex.findall(nasty + "; baz=bam")
            # whole bad pair reported in the 'invalid' group (the third one)
            assert result[0][2] == nasty
            # kept on truckin' and got the other one just fine.
            assert result[1] == ('baz', 'bam', '')
            # same thing if the good one is first and the bad one second
            result = regex.findall("baz=bam; " + nasty)
            assert result[0] == ('baz', 'bam', '')
            assert result[1][2] == ' ' + nasty

    def test_extension_av(self, check_unicode=False):
        """Test Definitions.EXTENSION_AV against extension-av per RFC 6265.

        extension-av      = <any CHAR except CTLs or ";">
        """
        # This is how it's defined in RFC 6265, just about verbatim.
        extension_av_explicit = "".join(sorted(
                set(RFC5234.CHAR) - set(RFC5234.CTL + ";")))
        # ... that should turn out to be the same as Definitions.EXTENSION_AV
        match = re.compile("^([%s]+)\Z" % Definitions.EXTENSION_AV).match
        # Verify I didn't mess up on escaping here first
        assert match(r']')
        assert match(r'[')
        assert match(r"'")
        assert match(r'"')
        assert match("\\")
        assert match(extension_av_explicit)
        # There should be some CHAR not matched
        assert not match(RFC5234.CHAR)
        # Every single CTL should not match
        for c in RFC5234.CTL + ";":
            assert not match(c)
        # Unicode over 7 bit ASCII shouldn't match, but this takes a while
        if check_unicode:
            for i in range(127, 0x10FFFF + 1):
                assert not match(unichr(i))

    def test_max_age_av(self):
        "Smoke test Definitions.MAX_AGE_AV"
        # Not a lot to this, it's just digits
        match = re.compile("^%s\Z" % Definitions.MAX_AGE_AV).match
        assert not match("")
        assert not match("Whiskers")
        assert not match("Max-Headroom=992")
        for c in "123456789":
            assert not match(c)
            assert match("Max-Age=%s" % c)
        assert match("Max-Age=0")
        for c in RFC5234.CHAR:
            assert not match(c)

    def test_label(self, check_unicode=False):
        "Test label, as used in Domain attribute"
        match = re.compile("^(%s)\Z" % Definitions.LABEL).match
        for i in range(0, 10):
            assert match(str(i))
        assert not match(".")
        assert not match(",")
        for c in RFC5234.CTL:
            assert not match("a%sb" % c)
            assert not match("%sb" % c)
            assert not match("a%s" % c)
        # Unicode over 7 bit ASCII shouldn't match, but this takes a while
        if check_unicode:
            for i in range(127, 0x10FFFF + 1):
                assert not match(unichr(i))

    def test_domain_av(self):
        "Smoke test Definitions.DOMAIN_AV"
        # This is basically just RFC1123.subdomain, which has its own
        # assertions in the class definition
        bad_domains = [
                ""
                ]
        good_domains = [
                "foobar.com",
                "foo-bar.com",
                "3Com.COM"
                ]

        # First test DOMAIN via DOMAIN_RE
        match = Definitions.DOMAIN_RE.match
        for domain in bad_domains:
            assert not match(domain)
        for domain in good_domains:
            assert match(domain)

        # Now same tests through DOMAIN_AV
        match = re.compile("^%s\Z" % Definitions.DOMAIN_AV).match
        for domain in bad_domains:
            assert not match("Domain=%s" % domain)
        for domain in good_domains:
            assert not match(domain)
            assert match("Domain=%s" % domain)
        # This is NOT valid and shouldn't be tolerated in cookies we create,
        # but it should be tolerated in existing cookies since people do it;
        # interpreted by stripping the initial .
        assert match("Domain=.foo.net")

    def test_path_av(self):
        "Smoke test PATH and PATH_AV"
        # This is basically just EXTENSION_AV, see test_extension_av
        bad_paths = [
                ""
                ]
        good_paths = [
                "/",
                "/foo",
                "/foo/bar"
                ]
        match = Definitions.PATH_RE.match
        for path in bad_paths:
            assert not match(path)
        for path in good_paths:
            assert match(path)

        match = re.compile("^%s\Z" % Definitions.PATH_AV).match
        for path in bad_paths:
            assert not match("Path=%s" % path)
        for path in good_paths:
            assert not match(path)
            assert match("Path=%s" % path)

    def test_months(self):
        """Sanity checks on MONTH_SHORT and MONTH_LONG month name recognizers.

        The RFCs set these in stone, they aren't locale-dependent.
        """
        match = re.compile(Definitions.MONTH_SHORT).match
        assert match("Jan")
        assert match("Feb")
        assert match("Mar")
        assert match("Apr")
        assert match("May")
        assert match("Jun")
        assert match("Jul")
        assert match("Aug")
        assert match("Sep")
        assert match("Oct")
        assert match("Nov")
        assert match("Dec")

        match = re.compile(Definitions.MONTH_LONG).match
        assert match("January")
        assert match("February")
        assert match("March")
        assert match("April")
        assert match("May")
        assert match("June")
        assert match("July")
        assert match("August")
        assert match("September")
        assert match("October")
        assert match("November")
        assert match("December")

    def test_weekdays(self):
        """Sanity check on WEEKDAY_SHORT and WEEKDAY_LONG weekday
        recognizers.

        The RFCs set these in stone, they aren't locale-dependent.
        """
        match = re.compile(Definitions.WEEKDAY_SHORT).match
        assert match("Mon")
        assert match("Tue")
        assert match("Wed")
        assert match("Thu")
        assert match("Fri")
        assert match("Sat")
        assert match("Sun")

        match = re.compile(Definitions.WEEKDAY_LONG).match
        assert match("Monday")
        assert match("Tuesday")
        assert match("Wednesday")
        assert match("Thursday")
        assert match("Friday")
        assert match("Saturday")
        assert match("Sunday")

    def test_day_of_month(self):
        """Check that the DAY_OF_MONTH regex allows all actual days, but
        excludes obviously wrong ones (so they are tossed in the first pass).
        """
        match = re.compile(Definitions.DAY_OF_MONTH).match
        for day in ['01', '02', '03', '04', '05', '06', '07', '08', '09', ' 1',
                ' 2', ' 3', ' 4', ' 5', ' 6', ' 7', ' 8', ' 9', '1', '2', '3',
                '4', '5', '6', '7', '8', '9'] \
                    + [str(i) for i in range(10, 32)]:
            assert match(day)
        assert not match("0")
        assert not match("00")
        assert not match("000")
        assert not match("111")
        assert not match("99")
        assert not match("41")

    def test_expires_av(self):
        "Smoke test the EXPIRES_AV regex pattern"
        # Definitions.EXPIRES_AV is actually pretty bad because it's a disaster
        # to test three different date formats with lots of definition
        # dependencies, and odds are good that other implementations are loose.
        # so this parser is also loose. "liberal in what you accept,
        # conservative in what you produce"
        match = re.compile("^%s\Z" % Definitions.EXPIRES_AV).match
        assert not match("")
        assert not match("Expires=")

        assert match("Expires=Tue, 15-Jan-2013 21:47:38 GMT")
        assert match("Expires=Sun, 06 Nov 1994 08:49:37 GMT")
        assert match("Expires=Sunday, 06-Nov-94 08:49:37 GMT")
        assert match("Expires=Sun Nov  6 08:49:37 1994")
        # attributed to Netscape in RFC 2109 10.1.2
        assert match("Expires=Mon, 13-Jun-93 10:00:00 GMT")

        assert not match("Expires=S9n, 06 Nov 1994 08:49:37 GMT")
        assert not match("Expires=Sun3ay, 06-Nov-94 08:49:37 GMT")
        assert not match("Expires=S9n Nov  6 08:49:37 1994")

        assert not match("Expires=Sun, A6 Nov 1994 08:49:37 GMT")
        assert not match("Expires=Sunday, 0B-Nov-94 08:49:37 GMT")
        assert not match("Expires=Sun No8  6 08:49:37 1994")

        assert not match("Expires=Sun, 06 N3v 1994 08:49:37 GMT")
        assert not match("Expires=Sunday, 06-N8v-94 08:49:37 GMT")
        assert not match("Expires=Sun Nov  A 08:49:37 1994")

        assert not match("Expires=Sun, 06 Nov 1B94 08:49:37 GMT")
        assert not match("Expires=Sunday, 06-Nov-C4 08:49:37 GMT")
        assert not match("Expires=Sun Nov  6 08:49:37 1Z94")

    def test_no_obvious_need_for_disjunctive_attr_pattern(self):
        """Smoke test the assumption that extension-av is a reasonable set of
        chars for all attrs (and thus that there is no reason to use a fancy
        disjunctive pattern in the findall that splits out the attrs, freeing
        us to use EXTENSION_AV instead).

        If this works, then ATTR should work
        """
        match = re.compile("^[%s]+\Z" % Definitions.EXTENSION_AV).match
        assert match("Expires=Sun, 06 Nov 1994 08:49:37 GMT")
        assert match("Expires=Sunday, 06-Nov-94 08:49:37 GMT")
        assert match("Expires=Sun Nov  6 08:49:37 1994")
        assert match("Max-Age=14658240962")
        assert match("Domain=FoO.b9ar.baz")
        assert match("Path=/flakes")
        assert match("Secure")
        assert match("HttpOnly")

    def test_attr(self):
        """Smoke test ATTR, used to compile ATTR_RE.
        """
        match = re.compile(Definitions.ATTR).match

        def recognized(pattern):
            "macro for seeing if ATTR recognized something"
            this_match = match(pattern)
            if not this_match:
                return False
            groupdict = this_match.groupdict()
            if groupdict['unrecognized']:
                return False
            return True

        # Quickly test that a batch of attributes matching the explicitly
        # recognized patterns make it through without anything in the
        # 'unrecognized' catchall capture group.
        for pattern in [
                "Secure",
                "HttpOnly",
                "Max-Age=9523052",
                "Domain=frobble.com",
                "Domain=3Com.COM",
                "Path=/",
                "Expires=Wed, 09 Jun 2021 10:18:14 GMT",
                ]:
            assert recognized(pattern)

        # Anything else is in extension-av and that's very broad;
        # see test_extension_av for that test.
        # This is only about the recognized ones.
        assert not recognized("Frob=mugmannary")
        assert not recognized("Fqjewp@1j5j510923")
        assert not recognized(";aqjwe")
        assert not recognized("ETJpqw;fjw")
        assert not recognized("fjq;")
        assert not recognized("Expires=\x00")

        # Verify interface from regexp for extracting values isn't changed;
        # a little rigidity here is a good idea
        expires = "Wed, 09 Jun 2021 10:18:14 GMT"
        m = match("Expires=%s" % expires)
        assert m.group("expires") == expires

        max_age = "233951698"
        m = match("Max-Age=%s" % max_age)
        assert m.group("max_age") == max_age

        domain = "flarp"
        m = match("Domain=%s" % domain)
        assert m.group("domain") == domain

        path = "2903"
        m = match("Path=%s" % path)
        assert m.group("path") == path

        m = match("Secure")
        assert m.group("secure")
        assert not m.group("httponly")

        m = match("HttpOnly")
        assert not m.group("secure")
        assert m.group("httponly")

    def test_date_accepts_formats(self):
        """Check that DATE matches most formats used in Expires: headers,
        and explain what the different formats are about.

        The value extraction of this regexp is more comprehensively exercised
        by test_date_parsing().
        """
        # Date formats vary widely in the wild. Even the standards vary widely.
        # This series of tests does spot-checks with instances of formats that
        # it makes sense to support. In the following comments, each format is
        # discussed and the rationale for the overall regexp is developed.

        match = re.compile(Definitions.DATE).match

        # The most common formats, related to the old Netscape cookie spec
        # (NCSP), are supposed to follow this template:
        #
        #   Wdy, DD-Mon-YYYY HH:MM:SS GMT
        #
        # (where 'Wdy' is a short weekday, and 'Mon' is a named month).
        assert match("Mon, 20-Jan-1994 00:00:00 GMT")

        # Similarly, RFC 850 proposes this format:
        #
        #   Weekday, DD-Mon-YY HH:MM:SS GMT
        #
        # (with a long-form weekday and a 2-digit year).
        assert match("Tuesday, 12-Feb-92 23:25:42 GMT")

        # RFC 1036 obsoleted the RFC 850 format:
        #
        #   Wdy, DD Mon YY HH:MM:SS GMT
        #
        # (shortening the weekday format and changing dashes to spaces).
        assert match("Wed, 30 Mar 92 13:16:12 GMT")

        # RFC 6265 cites a definition from RFC 2616, which uses the RFC 1123
        # definition but limits it to GMT (consonant with NCSP). RFC 1123
        # expanded RFC 822 with 2-4 digit years (more permissive than NCSP);
        # RFC 822 left weekday and seconds as optional, and a day of 1-2 digits
        # (all more permissive than NCSP). Giving something like this:
        #
        #   [Wdy, ][D]D Mon [YY]YY HH:MM[:SS] GMT
        #
        assert match("Thu, 3 Apr 91 12:46 GMT")
        # No weekday, two digit year.
        assert match("13 Apr 91 12:46 GMT")

        # Similarly, there is RFC 2822:
        #
        #   [Wdy, ][D]D Mon YYYY HH:MM[:SS] GMT
        # (which only differs in requiring a 4-digit year, where RFC  1123
        # permits 2 or 3 digit years).
        assert match("13 Apr 1991 12:46 GMT")
        assert match("Wed, 13 Apr 1991 12:46 GMT")

        # The generalized format given above encompasses RFC 1036 and RFC 2822
        # and would encompass NCSP except for the dashes; allowing long-form
        # weekdays also encompasses the format proposed in RFC 850. Taken
        # together, this should cover something like 99% of Expires values
        # (see, e.g., https://bugzilla.mozilla.org/show_bug.cgi?id=610218)

        # Finally, we also want to support asctime format, as mentioned in RFC
        # 850 and RFC 2616 and occasionally seen in the wild:
        #       Wdy Mon DD HH:MM:SS YYYY
        # e.g.: Sun Nov  6 08:49:37 1994
        assert match("Sun Nov  6 08:49:37 1994")
        assert match("Sun Nov 26 08:49:37 1994")
        # Reportedly someone has tacked 'GMT' on to the end of an asctime -
        # although this is not RFC valid, it is pretty harmless
        assert match("Sun Nov 26 08:49:37 1994 GMT")

        # This test is not passed until it is shown that it wasn't trivially
        # because DATE was matching .* or similar. This isn't intended to be
        # a thorough test, just rule out the obvious reason. See test_date()
        # for a more thorough workout of the whole parse and render mechanisms
        assert not match("")
        assert not match("       ")
        assert not match("wobbly")
        assert not match("Mon")
        assert not match("Mon, 20")
        assert not match("Mon, 20 Jan")
        assert not match("Mon, 20,Jan,1994 00:00:00 GMT")
        assert not match("Tuesday, 12-Feb-992 23:25:42 GMT")
        assert not match("Wed, 30 Mar 92 13:16:1210 GMT")
        assert not match("Wed, 30 Mar 92 13:16:12:10 GMT")
        assert not match("Thu, 3 Apr 91 12:461 GMT")

    def test_eol(self):
        """Test that the simple EOL regex works basically as expected.
        """
        split = Definitions.EOL.split
        assert split("foo\nbar") == ["foo", "bar"]
        assert split("foo\r\nbar") == ["foo", "bar"]
        letters = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
        assert split("\n".join(letters)) == letters
        assert split("\r\n".join(letters)) == letters

    def test_compiled(self):
        """Check that certain patterns are present as compiled regexps
        """
        re_type = type(re.compile(''))

        def present(name):
            "Macro for testing existence of an re in Definitions"
            item = getattr(Definitions, name)
            return item and isinstance(item, re_type)

        assert present("COOKIE_NAME_RE")
        assert present("COOKIE_RE")
        assert present("SET_COOKIE_HEADER_RE")
        assert present("ATTR_RE")
        assert present("DATE_RE")
        assert present("EOL")


def _test_init(cls, args, kwargs, expected):
    "Core instance test function for test_init"
    print("test_init", cls, args, kwargs)
    try:
        instance = cls(*args, **kwargs)
    except Exception as exception:
        if type(exception) == expected:
            return
        logging.error("expected %s, got %s", expected, repr(exception))
        raise
    if isinstance(expected, type) and issubclass(expected, Exception):
        raise AssertionError("No exception raised; "
        "expected %s for %s/%s" % (
            expected.__name__,
            repr(args),
            repr(kwargs)))
    for attr_name, attr_value in expected.items():
        assert getattr(instance, attr_name) == attr_value


class TestCookie(object):
    """Tests for the Cookie class.
    """
    # Test cases exercising different constructor calls to make a new Cookie
    # from scratch. Each case is tuple:
    # args, kwargs, exception or dict of expected attribute values
    # this exercises the default validators as well.
    creation_cases = [
            # bad call gives TypeError
            (("foo",), {}, TypeError),
            (("a", "b", "c"), {}, TypeError),
            # give un-ascii-able name - raises error due to likely
            # compatibility problems (cookie ignored, etc.)
            # in value it's fine, it'll be encoded and not inspected anyway.
            (("ăŊĻ", "b"), {}, InvalidCookieError),
            (("b", "ăŊĻ"), {}, {'name': 'b', 'value': "ăŊĻ"}),
            # normal simple construction gives name and value
            (("foo", "bar"), {}, {'name': 'foo', 'value': 'bar'}),
            # add a valid attribute and get it set
            (("baz", "bam"), {'max_age': 9},
                {'name': 'baz', 'value': 'bam', 'max_age': 9}),
            # multiple valid attributes
            (("x", "y"), {'max_age': 9, 'comment': 'fruity'},
                {'name': 'x', 'value': 'y',
                 'max_age': 9, 'comment': 'fruity'}),
            # invalid max-age
            (("w", "m"), {'max_age': 'loopy'}, InvalidCookieAttributeError),
            (("w", "m"), {'max_age': -1}, InvalidCookieAttributeError),
            (("w", "m"), {'max_age': 1.2}, InvalidCookieAttributeError),
            # invalid expires
            (("w", "m"), {'expires': 0}, InvalidCookieAttributeError),
            (("w", "m"), {'expires':
                datetime(2010, 1, 1, tzinfo=FixedOffsetTz(600))},
                InvalidCookieAttributeError),
            # control: valid expires
            (("w", "m"),
                {'expires': datetime(2010, 1, 1)},
                {'expires': datetime(2010, 1, 1)}),
            # invalid domain
            (("w", "m"), {'domain': ''}, InvalidCookieAttributeError),
            (("w", "m"), {'domain': '@'}, InvalidCookieAttributeError),
            (("w", "m"), {'domain': '.foo.net'}, {'domain': '.foo.net'}),
            # control: valid domain
            (("w", "m"),
                {'domain': 'foo.net'},
                {'domain': 'foo.net'},),
            # invalid path
            (("w", "m"), {'path': ''}, InvalidCookieAttributeError),
            (("w", "m"), {'path': '""'}, InvalidCookieAttributeError),
            (("w", "m"), {'path': 'foo'}, InvalidCookieAttributeError),
            (("w", "m"), {'path': '"/foo"'}, InvalidCookieAttributeError),
            (("w", "m"), {'path': ' /foo  '}, InvalidCookieAttributeError),
            # control: valid path
            (("w", "m"), {'path': '/'},
                    {'path': '/'}),
            (("w", "m"), {'path': '/axes'},
                    {'path': '/axes'}),
            # invalid version per RFC 2109/RFC 2965
            (("w", "m"), {'version': ''}, InvalidCookieAttributeError),
            (("w", "m"), {'version': 'baa'}, InvalidCookieAttributeError),
            (("w", "m"), {'version': -2}, InvalidCookieAttributeError),
            (("w", "m"), {'version': 2.3}, InvalidCookieAttributeError),
            # control: valid version
            (("w", "m"), {'version': 0}, {'version': 0}),
            (("w", "m"), {'version': 1}, {'version': 1}),
            (("w", "m"), {'version': 3042}, {'version': 3042}),
            # invalid secure, httponly
            (("w", "m"), {'secure': ''}, InvalidCookieAttributeError),
            (("w", "m"), {'secure': 0}, InvalidCookieAttributeError),
            (("w", "m"), {'secure': 1}, InvalidCookieAttributeError),
            (("w", "m"), {'secure': 'a'}, InvalidCookieAttributeError),
            (("w", "m"), {'httponly': ''}, InvalidCookieAttributeError),
            (("w", "m"), {'httponly': 0}, InvalidCookieAttributeError),
            (("w", "m"), {'httponly': 1}, InvalidCookieAttributeError),
            (("w", "m"), {'httponly': 'a'}, InvalidCookieAttributeError),
            # valid comment
            (("w", "m"), {'comment': 'a'}, {'comment': 'a'}),
            # invalid names
            # (unicode cases are done last because they mess with pytest print)
            ((None, "m"), {}, InvalidCookieError),
            (("", "m"), {}, InvalidCookieError),
            (("ü", "m"), {}, InvalidCookieError),
            # invalid values
            (("w", None), {}, {'name': 'w'}),
            # a control - unicode is valid value, just gets encoded on way out
            (("w", "üm"), {}, {'value': "üm"}),
            # comma
            (('a', ','), {}, {'value': ','}),
            # semicolons
            (('a', ';'), {}, {'value': ';'}),
            # spaces
            (('a', ' '), {}, {'value': ' '}),
            ]

    def test_init(self):
        """Exercise __init__ and validators.

        This is important both because it is a user-facing API, and also
        because the parse/render tests depend heavily on it.
        """
        creation_cases = self.creation_cases + [
            (("a", "b"), {'frob': 10}, InvalidCookieAttributeError)
            ]
        counter = 0
        for args, kwargs, expected in creation_cases:
            counter += 1
            logging.error("counter %d, %s, %s, %s", counter, args, kwargs,
                    expected)
            _test_init(Cookie, args, kwargs, expected)

    def test_set_attributes(self):
        """Exercise setting, validation and getting of attributes without
        much involving __init__. Also sets value and name.
        """
        for args, kwargs, expected in self.creation_cases:
            if not kwargs:
                continue
            try:
                cookie = Cookie("yarp", "flam")
                for attr, value in kwargs.items():
                    setattr(cookie, attr, value)
                if args:
                    cookie.name = args[0]
                    cookie.value = args[1]
            except Exception as e:
                if type(e) == expected:
                    continue
                raise
            if isinstance(expected, type) and issubclass(expected, Exception):
                raise AssertionError("No exception raised; "
                "expected %s for %s" % (
                    expected.__name__,
                    repr(kwargs)))
            for attr_name, attr_value in expected.items():
                assert getattr(cookie, attr_name) == attr_value

    def test_get_defaults(self):
        "Test that defaults are right for cookie attrs"
        cookie = Cookie("foo", "bar")
        for attr in (
                "expires",
                "max_age",
                "domain",
                "path",
                "comment",
                "version",
                "secure",
                "httponly"):
            assert hasattr(cookie, attr)
            assert getattr(cookie, attr) == None
        # Verify that not every name is getting something
        for attr in ("foo", "bar", "baz"):
            assert not hasattr(cookie, attr)
            with raises(AttributeError):
                getattr(cookie, attr)

    names_values = [
        ("a", "b"),
        ("foo", "bar"),
        ("baz", "1234567890"),
        ("!!#po99!", "blah"),
        ("^_~`*", "foo"),
        ("%s+|-.&$", "snah"),
        ("lub", "!@#$%^&*()[]{}|/:'<>~.?`"),
        ("woah", "====+-_"),
        ]

    def test_render_response(self):
        "Test rendering Cookie object for Set-Cookie: header"
        for name, value in self.names_values:
            cookie = Cookie(name, value)
            expected = "{name}={value}".format(
                    name=name, value=value)
            assert cookie.render_response() == expected
        for data, result in [
                ({'name': 'a', 'value': 'b'}, "a=b"),
                ({'name': 'foo', 'value': 'bar'}, "foo=bar"),
                ({'name': 'baz', 'value': 'bam'}, "baz=bam"),
                ({'name': 'baz', 'value': 'bam', 'max_age': 2},
                    "baz=bam; Max-Age=2"),
                ({'name': 'baz', 'value': 'bam',
                    'max_age': 2, 'comment': 'foobarbaz'},
                    "baz=bam; Max-Age=2; Comment=foobarbaz"),
                ({'name': 'baz', 'value': 'bam',
                    'max_age': 2,
                    'expires': datetime(1970, 1, 1),
                    },
                    "baz=bam; Max-Age=2; "
                    "Expires=Thu, 01 Jan 1970 00:00:00 GMT"),
                ({'name': 'baz', 'value': 'bam', 'path': '/yams',
                    'domain': '3Com.COM'},
                    "baz=bam; Domain=3Com.COM; Path=/yams"),
                ({'name': 'baz', 'value': 'bam', 'path': '/', 'secure': True,
                    'httponly': True},
                    "baz=bam; Path=/; Secure; HttpOnly"),
                ({'name': 'baz', 'value': 'bam', 'domain': '.domain'},
                    'baz=bam; Domain=domain'),
                ]:
            cookie = Cookie(**data)
            actual = sorted(cookie.render_response().split("; "))
            ideal = sorted(result.split("; "))
            assert actual == ideal

    def test_render_encode(self):
        """Test encoding of a few special characters.

        as in http://bugs.python.org/issue9824
        """
        cases = {
                ("x", "foo,bar;baz"): 'x=foo%2Cbar%3Bbaz',
                ("y", 'yap"bip'): 'y=yap%22bip',
                }
        for args, ideal in cases.items():
            cookie = Cookie(*args)
            assert cookie.render_response() == ideal
            assert cookie.render_request() == ideal

    def test_legacy_quotes(self):
        """Check that cookies which delimit values with quotes are understood
        but that this non-6265 behavior is not repeated in the output
        """
        cookie = Cookie.from_string(
                'Set-Cookie: y="foo"; version="1"; Path="/foo"')
        assert cookie.name == 'y'
        assert cookie.value == 'foo'
        assert cookie.version == 1
        assert cookie.path == "/foo"
        pieces = cookie.render_response().split("; ")
        assert pieces[0] == 'y=foo'
        assert set(pieces[1:]) == set([
            'Path=/foo', 'Version=1'
            ])

    def test_render_response_expires(self):
        "Simple spot check of cookie expires rendering"
        a = Cookie('a', 'blah')
        a.expires = parse_date("Wed, 23-Jan-1992 00:01:02 GMT")
        assert a.render_response() == \
                'a=blah; Expires=Thu, 23 Jan 1992 00:01:02 GMT'

        b = Cookie('b', 'blr')
        b.expires = parse_date("Sun Nov  6 08:49:37 1994")
        assert b.render_response() == \
                'b=blr; Expires=Sun, 06 Nov 1994 08:49:37 GMT'

    def test_eq(self):
        "Smoke test equality/inequality with Cookie objects"
        ref = Cookie('a', 'b')
        # trivial cases
        assert ref == ref
        assert not (ref != ref)
        assert None != ref
        assert not (None == ref)
        assert ref != None
        assert not (ref == None)
        # equivalence and nonequivalence
        assert Cookie('a', 'b') is not ref
        assert Cookie('a', 'b') == ref
        assert Cookie('x', 'y') != ref
        assert Cookie('a', 'y') != ref
        assert Cookie('a', 'b', path='/') != ref
        assert {'c': 'd'} != ref
        assert ref != {'c': 'd'}
        # unlike attribute values and sets of attributes
        assert Cookie('a', 'b', path='/a') \
                != Cookie('a', 'b', path='/')
        assert Cookie('x', 'y', max_age=3) != \
                Cookie('x', 'y', path='/b')
        assert Cookie('yargo', 'z', max_age=5) != \
                Cookie('yargo', 'z', max_age=6)
        assert ref != Cookie('a', 'b', domain='yab')
        # Exercise bytes conversion
        assert Cookie(b'a', 'b') == Cookie('a', 'b')
        assert Cookie(b'a', 'b') == Cookie(b'a', 'b')

    def test_manifest(self):
        "Test presence of important stuff on Cookie class"
        for name in ("attribute_names", "attribute_renderers",
                "attribute_parsers", "attribute_validators"):
            dictionary = getattr(Cookie, name)
            assert dictionary
            assert isinstance(dictionary, dict)

    def test_simple_extension(self):
        "Trivial example/smoke test of extending Cookie"

        count_state = [0]

        def call_counter(item=None):
            count_state[0] += 1
            return True if item else False

        class Cookie2(Cookie):
            "Example Cookie subclass with new behavior"
            attribute_names = {
                    'foo': 'Foo',
                    'bar': 'Bar',
                    'baz': 'Baz',
                    'ram': 'Ram',
                    }
            attribute_parsers = {
                    'foo': lambda s: "/".join(s),
                    'bar': call_counter,
                    'value': lambda s:
                             parse_value(s, allow_spaces=True),
                    }
            attribute_validators = {
                    'foo': lambda item: True,
                    'bar': call_counter,
                    'baz': lambda item: False,
                    }
            attribute_renderers = {
                    'foo': lambda s: "|".join(s) if s else None,
                    'bar': call_counter,
                    'name': lambda item: item,
                    }
        cookie = Cookie2("a", "b")
        for key in Cookie2.attribute_names:
            assert hasattr(cookie, key)
            assert getattr(cookie, key) == None
        cookie.foo = "abc"
        assert cookie.render_request() == "a=b"
        assert cookie.render_response() == "a=b; Foo=a|b|c"
        cookie.foo = None
        # Setting it to None makes it drop from the listing
        assert cookie.render_response() == "a=b"

        cookie.bar = "what"
        assert cookie.bar == "what"
        assert cookie.render_request() == "a=b"
        # bar's renderer returns a bool; if it's True we get Bar.
        # that's a special case for flags like HttpOnly.
        assert cookie.render_response() == "a=b; Bar"

        with raises(InvalidCookieAttributeError):
            cookie.baz = "anything"

        Cookie2('a', 'b fog')
        Cookie2('a', ' b=fo g')

    def test_from_string(self):
        with raises(InvalidCookieError):
            Cookie.from_string("")
        with raises(InvalidCookieError):
            Cookie.from_string("", ignore_bad_attributes=True)
        assert Cookie.from_string("", ignore_bad_cookies=True) == None

    def test_from_dict(self):
        assert Cookie.from_dict({'name': 'a', 'value': 'b'}) == \
                Cookie('a', 'b')
        assert Cookie.from_dict(
                {'name': 'a', 'value': 'b', 'duh': 'no'},
                ignore_bad_attributes=True) == \
                Cookie('a', 'b')
        with raises(InvalidCookieError):
            Cookie.from_dict({}, ignore_bad_attributes=True)
        with raises(InvalidCookieError):
            Cookie.from_dict({}, ignore_bad_attributes=False)
        with raises(InvalidCookieError):
            Cookie.from_dict({'name': ''}, ignore_bad_attributes=False)
        with raises(InvalidCookieError):
            Cookie.from_dict({'name': None, 'value': 'b'},
                    ignore_bad_attributes=False)
        assert Cookie.from_dict({'name': 'foo'}) == Cookie('foo', None)
        assert Cookie.from_dict({'name': 'foo', 'value': ''}) == \
                Cookie('foo', None)
        with raises(InvalidCookieAttributeError):
            assert Cookie.from_dict(
                    {'name': 'a', 'value': 'b', 'duh': 'no'},
                    ignore_bad_attributes=False)
        assert Cookie.from_dict({'name': 'a', 'value': 'b', 'expires': 2},
            ignore_bad_attributes=True) == Cookie('a', 'b')
        with raises(InvalidCookieAttributeError):
            assert Cookie.from_dict({'name': 'a', 'value': 'b', 'expires': 2},
                ignore_bad_attributes=False)


class Scone(object):
    """Non-useful alternative to Cookie class for tests only.
    """
    def __init__(self, name, value):
        self.name = name
        self.value = value

    @classmethod
    def from_dict(cls, cookie_dict):
        instance = cls(cookie_dict['name'], cookie_dict['value'])
        return instance

    def __eq__(self, other):
        if type(self) != type(other):
            return False
        if self.name != other.name:
            return False
        if self.value != other.value:
            return False
        return True


class Scones(Cookies):
    """Non-useful alternative to Cookies class for tests only.
    """
    DEFAULT_COOKIE_CLASS = Scone


class TestCookies(object):
    """Tests for the Cookies class.
    """
    creation_cases = [
            # Only args - simple
            ((Cookie("a", "b"),), {}, 1),
            # Only kwargs - simple
            (tuple(), {'a': 'b'}, 1),
            # Only kwargs - bigger
            (tuple(),
                {'axl': 'bosk',
                 'x': 'y',
                 'foo': 'bar',
                 'baz': 'bam'}, 4),
            # Sum between args/kwargs
            ((Cookie('a', 'b'),),
                {'axl': 'bosk',
                 'x': 'y',
                 'foo': 'bar',
                 'baz': 'bam'}, 5),
            # Redundant between args/kwargs
            ((Cookie('a', 'b'),
              Cookie('x', 'y')),
                {'axl': 'bosk',
                 'x': 'y',
                 'foo': 'bar',
                 'baz': 'bam'}, 5),
            ]

    def test_init(self):
        """Create some Cookies objects with __init__, varying the constructor
        arguments, and check on the results.

        Exercises __init__, __repr__, render_request, render_response, and
        simple cases of parse_response and parse_request.
        """
        def same(a, b):
            keys = sorted(set(a.keys() + b.keys()))
            for key in keys:
                assert a[key] == b[key]

        for args, kwargs, length in self.creation_cases:
            # Make a Cookies object using the args.
            cookies = Cookies(*args, **kwargs)
            assert len(cookies) == length

            # Render into various text formats.
            rep = repr(cookies)
            res = cookies.render_response()
            req = cookies.render_request()

            # Very basic sanity check on renders, fail fast and in a simple way
            # if output is truly terrible
            assert rep.count('=') == length
            assert len(res) == length
            assert [item.count('=') == 1 for item in res]
            assert req.count('=') == length
            assert len(req.split(";")) == length

            # Explicitly parse out the data (this can be simple since the
            # output should be in a highly consistent format)
            pairs = [item.split("=") for item in req.split("; ")]
            assert len(pairs) == length
            for name, value in pairs:
                cookie = cookies[name]
                assert cookie.name == name
                assert cookie.value == value

            # Parse the rendered output, check that result is equal to the
            # originally produced object.

            parsed = Cookies()
            parsed.parse_request(req)
            assert parsed == cookies

            parsed = Cookies()
            for item in res:
                parsed.parse_response(item)
            assert parsed == cookies

            # Check that all the requested cookies were created correctly:
            # indexed with correct names in dict, also with correctly set name
            # and value attributes.
            for cookie in args:
                assert cookies[cookie.name] == cookie
            for name, value in kwargs.items():
                cookie = cookies[name]
                assert cookie.name == name
                assert cookie.value == value
                assert name in rep
                assert value in rep

            # Spot check that setting an attribute still works
            # with these particular parameters. Not a torture test.
            for key in cookies:
                cookies[key].max_age = 42
            for line in cookies.render_response():
                assert line.endswith("Max-Age=42")

            # Spot check attribute deletion
            assert cookies[key].max_age
            del cookies[key].max_age
            assert cookies[key].max_age is None

            # Spot check cookie deletion
            keys = [key for key in cookies.keys()]
            for key in keys:
                del cookies[key]
                assert key not in cookies

    def test_eq(self):
        "Smoke test equality/inequality of Cookies objects"
        ref = Cookies(a='b')
        assert Cookies(a='b') == ref
        assert Cookies(b='c') != ref
        assert ref != Cookies(d='e')
        assert Cookies(a='x') != ref

        class Dummy(object):
            "Just any old object"
            pass
        x = Dummy()
        x.keys = True
        with raises(TypeError):
            assert ref != x

    def test_add(self):
        "Test the Cookies.add method"
        for args, kwargs, length in self.creation_cases:
            cookies = Cookies()
            cookies.add(*args, **kwargs)
            assert len(cookies) == length
            for cookie in args:
                assert cookies[cookie.name] == cookie
            for name, value in kwargs.items():
                cookie = cookies[name]
                assert cookie.value == value
            count = len(cookies)
            assert 'w' not in cookies
            cookies.add(w='m')
            assert 'w' in cookies
            assert count == len(cookies) - 1
            assert cookies['w'].value == 'm'

    def test_empty(self):
        "Trivial test of behavior of empty Cookies object"
        cookies = Cookies()
        assert len(cookies) == 0
        assert Cookies() == cookies

    def test_parse_request(self):
        """Test Cookies.parse_request.
        """
        def run(arg, **kwargs):
            "run Cookies.parse_request on an instance"
            cookies = Cookies()
            result = runner(cookies.parse_request)(arg, **kwargs)
            return result

        for i, case in enumerate(HEADER_CASES):
            arg, kwargs, expected, response_result = case

            # parse_request doesn't take ignore_bad_attributes. remove it
            # without changing original kwargs for further tests
            kwargs = kwargs.copy()
            if 'ignore_bad_attributes' in kwargs:
                del kwargs['ignore_bad_attributes']

            def expect(arg, kwargs):
                "repeated complex assertion"
                result = run(arg, **kwargs)
                assert result == expected \
                       or isinstance(expected, type) \
                       and type(result) == expected, \
                        "unexpected result for (%s): %s. should be %s" \
                        % (repr(arg), repr(result), repr(expected))

            # Check result - should be same with and without the prefix
            expect("Cookie: " + arg, kwargs)
            expect(arg, kwargs)

            # But it should not match with the response prefix.
            other_result = run("Set-Cookie: " + arg, **kwargs)
            assert other_result != expected
            assert other_result != response_result

            # If case expects InvalidCookieError, verify that it is suppressed
            # by ignore_bad_cookies.
            if expected == InvalidCookieError:
                kwargs2 = kwargs.copy()
                kwargs2['ignore_bad_cookies'] = True
                cookies = Cookies()
                # Let natural exception raise, easier to figure out
                cookies.parse_request(arg, **kwargs2)

        # Spot check that exception is raised for clearly wrong format
        assert not isinstance(run("Cookie: a=b"), InvalidCookieError)
        assert isinstance(run("Set-Cookie: a=b"), InvalidCookieError)

    def test_parse_response(self):
        """Test Cookies.parse_response.
        """
        def run(arg, **kwargs):
            "run parse_response method of a Cookies instance"
            cookies = Cookies()
            return runner(cookies.parse_response)(arg, **kwargs)

        for case in HEADER_CASES:
            arg, kwargs, request_result, expected = case
            # If we expect InvalidCookieError or InvalidCookieAttributeError,
            # telling the function to ignore those should result in no
            # exception.
            kwargs2 = kwargs.copy()
            if expected == InvalidCookieError:
                kwargs2['ignore_bad_cookies'] = True
                assert not isinstance(
                        run(arg, **kwargs2),
                        Exception)
            elif expected == InvalidCookieAttributeError:
                kwargs2['ignore_bad_attributes'] = True
                result = run(arg, **kwargs2)
                if isinstance(result, InvalidCookieAttributeError):
                    raise AssertionError("InvalidCookieAttributeError "
                        "should have been silenced/logged")
                else:
                    assert not isinstance(result, Exception)
            # Check result - should be same with and without the prefix
            sys.stdout.flush()
            result = run(arg, **kwargs)
            assert result == expected \
                    or isinstance(expected, type) \
                    and type(result) == expected, \
                    "unexpected result for (%s): %s. should be %s" \
                    % (repr(arg), repr(result), repr(expected))
            result = run("Set-Cookie: " + arg, **kwargs)
            assert result == expected \
                    or isinstance(expected, type) \
                    and type(result) == expected, \
                    "unexpected result for (%s): %s. should be %s" \
                    % (repr("Set-Cookie: " + arg),
                       repr(result), repr(expected))
            # But it should not match with the request prefix.
            other_result = run("Cookie: " + arg, **kwargs)
            assert other_result != expected
            assert other_result != request_result

        assert not isinstance(run("Set-Cookie: a=b"), InvalidCookieError)
        assert isinstance(run("Cookie: a=b"), InvalidCookieError)

    def test_exercise_parse_one_response_asctime(self):
        asctime = 'Sun Nov  6 08:49:37 1994'
        line = "Set-Cookie: a=b; Expires=%s" % asctime
        response_dict = parse_one_response(line)
        assert response_dict == \
            {'expires': 'Sun Nov  6 08:49:37 1994', 'name': 'a', 'value': 'b'}
        assert Cookie.from_dict(response_dict) == \
                Cookie('a', 'b', expires=parse_date(asctime))

    def test_get_all(self):
        cookies = Cookies.from_request('a=b; a=c; b=x')
        assert cookies['a'].value == 'b'
        assert cookies['b'].value == 'x'
        values = [cookie.value for cookie in cookies.get_all('a')]
        assert values == ['b', 'c']

    def test_custom_cookie_class_on_instance(self):
        cookies = Cookies(_cookie_class=Scone)
        cookies.add(a="b")
        assert cookies['a'] == Scone("a", "b")

    def test_custom_cookie_class_on_subclass(self):
        cookies = Scones()
        cookies.add(a="b")
        assert cookies['a'] == Scone("a", "b")

    def test_custom_cookie_class_on_instance_parse_request(self):
        cookies = Scones()
        cookies.parse_request("Cookie: c=d")
        assert cookies['c'] == Scone("c", "d")

    def test_custom_cookie_class_on_instance_parse_response(self):
        cookies = Scones()
        cookies.parse_response("Set-Cookie: c=d")
        assert cookies['c'] == Scone("c", "d")


def test_parse_date():
    """Throw a ton of dirty samples at the date parse/render and verify the
    exact output of rendering the parsed version of the sample.
    """
    cases = [
        # Obviously off format
        ("", None),
        ("    ", None),
        ("\t", None),
        ("\n", None),
        ("\x02\x03\x04", None),
        ("froppity", None),
        ("@@@@@%@#:%", None),
        ("foo bar baz", None),
        # We'll do a number of overall manglings.
        # First, show that the baseline passes
        ("Sat, 10 Oct 2009 13:47:21 GMT", "Sat, 10 Oct 2009 13:47:21 GMT"),
        # Delete semantically important pieces
        (" Oct 2009 13:47:21 GMT", None),
        ("Fri, Oct 2009 13:47:21 GMT", None),
        ("Fri, 10 2009 13:47:21 GMT", None),
        ("Sat, 10 Oct 2009 :47:21 GMT", None),
        ("Sat, 10 Oct 2009 13::21 GMT", None),
        ("Sat, 10 Oct 2009 13:47: GMT", None),
        # Replace single characters out of tokens with spaces - harder to
        # do programmatically because some whitespace can reasonably be
        # tolerated.
        ("F i, 10 Oct 2009 13:47:21 GMT", None),
        ("Fr , 10 Oct 2009 13:47:21 GMT", None),
        ("Fri, 10  ct 2009 13:47:21 GMT", None),
        ("Fri, 10 O t 2009 13:47:21 GMT", None),
        ("Fri, 10 Oc  2009 13:47:21 GMT", None),
        ("Sat, 10 Oct  009 13:47:21 GMT", None),
        ("Sat, 10 Oct 2 09 13:47:21 GMT", None),
        ("Sat, 10 Oct 20 9 13:47:21 GMT", None),
        ("Sat, 10 Oct 200  13:47:21 GMT", None),
        ("Sat, 10 Oct 2009 1 :47:21 GMT", None),
        ("Sat, 10 Oct 2009 13 47:21 GMT", None),
        ("Sat, 10 Oct 2009 13: 7:21 GMT", None),
        ("Sat, 10 Oct 2009 13:4 :21 GMT", None),
        ("Sat, 10 Oct 2009 13:47 21 GMT", None),
        ("Sat, 10 Oct 2009 13:47: 1 GMT", None),
        ("Sat, 10 Oct 2009 13:47:2  GMT", None),
        ("Sat, 10 Oct 2009 13:47:21  MT", None),
        ("Sat, 10 Oct 2009 13:47:21 G T", None),
        ("Sat, 10 Oct 2009 13:47:21 GM ", None),
        # Replace numeric elements with stuff that contains A-Z
        ("Fri, Burp Oct 2009 13:47:21 GMT", None),
        ("Fri, 10 Tabalqplar 2009 13:47:21 GMT", None),
        ("Sat, 10 Oct Fruit 13:47:21 GMT", None),
        ("Sat, 10 Oct 2009 13:47:21 Fruits", None),
        # Weekday
        (", Dec 31 00:00:00 2003", None),
        ("T, Dec 31 00:00:00 2003", None),
        ("Tu, Dec 31 00:00:00 2003", None),
        ("Hi, Dec 31 00:00:00 2003", None),
        ("Heretounforeseen, Dec 31 00:00:00 2003", None),
        ("Wednesday2, Dec 31 00:00:00 2003", None),
        ("Mon\x00frobs, Dec 31 00:00:00 2003", None),
        ("Mon\x10day, Dec 31 00:00:00 2003", None),
        # Day of month
        ("Fri, Oct 2009 13:47:21 GMT", None),
        ("Fri, 110 Oct 2009 13:47:21 GMT", None),
        ("Fri, 0 Oct 2009 13:47:21 GMT", None),
        ("Fri, 00 Oct 2009 13:47:21 GMT", None),
        ("Fri, 0  Oct 2009 13:47:21 GMT", None),
        ("Fri,  0 Oct 2009 13:47:21 GMT", None),
        ("Fri, 00 Oct 2009 13:47:21 GMT", None),
        ("Fri, 33 Oct 2009 13:47:21 GMT", None),
        ("Fri, 40 Oct 2009 13:47:21 GMT", None),
        ("Fri, A2 Oct 2009 13:47:21 GMT", None),
        ("Fri, 2\x00 Oct 2009 13:47:21 GMT", None),
        ("Fri, \t3 Oct 2009 13:47:21 GMT", None),
        ("Fri, 3\t Oct 2009 13:47:21 GMT", None),
        # Month
        ("Fri, 10  2009 13:47:21 GMT", None),
        ("Fri, 10 O 2009 13:47:21 GMT", None),
        ("Fri, 10 Oc 2009 13:47:21 GMT", None),
        ("Sat, 10 Octuarial 2009 13:47:21 GMT", None),
        ("Sat, 10 Octuary 2009 13:47:21 GMT", None),
        ("Sat, 10 Octubre 2009 13:47:21 GMT", None),
        # Year
        ("Sat, 10 Oct 009 13:47:21 GMT", None),
        ("Sat, 10 Oct 200 13:47:21 GMT", None),
        ("Sat, 10 Oct 209 13:47:21 GMT", None),
        ("Sat, 10 Oct 20 9 13:47:21 GMT", None),
        # Hour
        ("Sat, 10 Oct 2009 25:47:21 GMT", None),
        ("Sat, 10 Oct 2009 1@:47:21 GMT", None),
        # Minute
        ("Sat, 10 Oct 2009 13:71:21 GMT", None),
        ("Sat, 10 Oct 2009 13:61:21 GMT", None),
        ("Sat, 10 Oct 2009 13:60:21 GMT", None),
        ("Sat, 10 Oct 2009 24:01:00 GMT", None),
        # Second
        ("Sat, 10 Oct 2009 13:47 GMT", "Sat, 10 Oct 2009 13:47:00 GMT"),
        ("Sat, 10 Oct 2009 13:47:00 GMT", "Sat, 10 Oct 2009 13:47:00 GMT"),
        ("Sat, 10 Oct 2009 24:00:01 GMT", None),
        # Some reasonable cases (ignore weekday)
        ("Mon Dec 24 16:32:39 1977 GMT", "Sat, 24 Dec 1977 16:32:39 GMT"),
        ("Sat, 7 Dec 1991 13:56:05 GMT", "Sat, 07 Dec 1991 13:56:05 GMT"),
        ("Saturday, 8-Mar-2012 21:35:09 GMT", "Thu, 08 Mar 2012 21:35:09 GMT"),
        ("Sun, 1-Feb-1998 00:00:00 GMT", "Sun, 01 Feb 1998 00:00:00 GMT"),
        ("Thursday, 01-Jan-1983 01:01:01 GMT",
                "Sat, 01 Jan 1983 01:01:01 GMT"),
        ("Tue, 15-Nov-1973 22:23:24 GMT", "Thu, 15 Nov 1973 22:23:24 GMT"),
        ("Wed, 09 Dec 1999 23:59:59 GMT", "Thu, 09 Dec 1999 23:59:59 GMT"),
        ("Mon, 12-May-05 20:25:03 GMT", "Thu, 12 May 2005 20:25:03 GMT"),
        ("Thursday, 01-Jan-12 09:00:00 GMT", "Sun, 01 Jan 2012 09:00:00 GMT"),
        # starts like asctime, but flips the time and year - nonsense
        ("Wed Mar 12 2007 08:25:07 GMT", None),
        # starts like RFC 1123, but flips the time and year - nonsense
        ("Thu, 31 Dec 23:55:55 2107 GMT", None),
        ('Fri, 21-May-2004 10:40:51 GMT', "Fri, 21 May 2004 10:40:51 GMT"),
        # extra 2-digit year exercises
        ("Sat, 10 Oct 11 13:47:21 GMT", "Mon, 10 Oct 2011 13:47:21 GMT"),
        ("Sat, 10 Oct 09 13:47:22 GMT", "Sat, 10 Oct 2009 13:47:22 GMT"),
        ("Sat, 10 Oct 93 13:47:23 GMT", "Sun, 10 Oct 1993 13:47:23 GMT"),
        ("Sat, 10 Oct 85 13:47:24 GMT", "Thu, 10 Oct 1985 13:47:24 GMT"),
        ("Sat, 10 Oct 70 13:47:25 GMT", "Sat, 10 Oct 1970 13:47:25 GMT"),
        ("Sat, 10 Oct 69 13:47:26 GMT", "Thu, 10 Oct 2069 13:47:26 GMT"),
        # dealing with 3-digit year is incredibly tedious, will do as needed
        ("Sat, 10 Oct 969 13:47:26 GMT", None),
        ("Sat, 10 Oct 9 13:47:26 GMT", None),
        ("Fri, 10 Oct 19691 13:47:26 GMT", None),
    ]

    def change(string, position, new_value):
        "Macro to change a string"
        return string[:position] + new_value + string[position + 1:]

    original = "Sat, 10 Oct 2009 13:47:21 GMT"

    # Stuff garbage in every position - none of these characters should
    # ever be allowed in a date string.
    # not included because pytest chokes: "¿�␦"
    bad_chars = "/<>()\\*$#&=;\x00\b\f\n\r\"\'`?"
    for pos in range(0, len(original)):
        for bad_char in bad_chars:
            cases.append((change(original, pos, bad_char), None))

    # Invalidate each letter
    letter_positions = [i for (i, c) in enumerate(original)  \
                        if re.match("[A-Za-z]", c)]
    for pos in letter_positions:
        cases.append((change(original, pos, 'q'), None))
        cases.append((change(original, pos, '0'), None))
        cases.append((change(original, pos, '-'), None))
        cases.append((change(original, pos, ''), None))
        # But do tolerate case changes.
        c = original[pos]
        if c.isupper():
            c = c.lower()
        else:
            c = c.upper()
        cases.append((change(original, pos, c), original))

    # Invalidate each digit
    digit_positions = [i for (i, c) in enumerate(original) \
                       if c in "0123456789"]
    for pos in digit_positions:
        c = original[pos]
        cases.append((change(original, pos, 'q'), None))
        cases.append((change(original, pos, '-' + c), None))
        cases.append((change(original, pos, '+' + c), None))

    # Invalidate each space
    space_positions = [i for (i, c) in enumerate(original) \
                       if c in " \t\n\r"]
    for pos in space_positions:
        cases.append((change(original, pos, 'x'), None))
        cases.append((change(original, pos, '\t'), None))
        cases.append((change(original, pos, '  '), None))
        cases.append((change(original, pos, ''), None))

    # Invalidate each colon
    colon_positions = [i for (i, c) in enumerate(original) \
                       if c == ":"]
    for pos in colon_positions:
        cases.append((change(original, pos, 'z'), None))
        cases.append((change(original, pos, '0'), None))
        cases.append((change(original, pos, ' '), None))
        cases.append((change(original, pos, ''), None))

    for data, ideal in cases:
        actual = render_date(parse_date(data))
        assert actual == ideal


def runner(function):
    """Generate a function which collects the result/exception from another
    function, for easier assertions.
    """
    def run(*args, **kwargs):
        "Function which collects result/exception"
        actual_result, actual_exception = None, None
        try:
            actual_result = function(*args, **kwargs)
        except Exception as exception:
            actual_exception = exception
        return actual_exception or actual_result
    return run


# Define cases for testing parsing and rendering.
# Format: input, kwargs, expected parse_request result, expected parse_response
# result.

HEADER_CASES = [
        # cases with nothing that can be parsed out result in
        # InvalidCookieError. unless ignore_bad_cookies=True, then they give an
        # empty Cookies().
        ("", {},
            InvalidCookieError,
            InvalidCookieError),
        ('a', {},
            InvalidCookieError,
            InvalidCookieError),
        ("    ", {},
            InvalidCookieError,
            InvalidCookieError),
        (";;;;;", {},
            InvalidCookieError,
            InvalidCookieError),
        ("qwejrkqlwjere", {},
            InvalidCookieError,
            InvalidCookieError),
        # vacuous headers should give invalid
        ('Cookie: ', {},
            InvalidCookieError,
            InvalidCookieError),
        ('Set-Cookie: ', {},
            InvalidCookieError,
            InvalidCookieError),
        # Single pair should work the same as request or response
        ("foo=bar", {},
            Cookies(foo='bar'),
            Cookies(foo='bar')),
        ("SID=242d96421d4e", {},
            Cookies(SID='242d96421d4e'),
            Cookies(SID='242d96421d4e')),
        # Two pairs on SAME line should work with request, fail with response.
        # if ignore_bad_attributes, response should not raise.
        # and ignore_bad_attributes behavior should be default
        ("a=b; c=dx", {'ignore_bad_attributes': True},
            Cookies(a='b', c='dx'),
            Cookies(a='b')),
        ("a=b; c=d", {'ignore_bad_attributes': False},
            Cookies(a='b', c='d'),
            InvalidCookieAttributeError),
        ('g=h;j=k', {},
            Cookies(g='h', j='k'),
            Cookies(g='h')),
        # tolerance: response shouldn't barf on unrecognized attr by default,
        # but request should recognize as malformed
        ('a=b; brains', {},
            InvalidCookieError,
            Cookies(a='b')),
        # tolerance: should strip quotes and spaces
        ('A="BBB"', {},
            Cookies(A='BBB'),
            Cookies(A='BBB'),
            ),
        ('A=  "BBB"  ', {},
            Cookies(A='BBB'),
            Cookies(A='BBB'),
            ),
        # tolerance: should ignore dumb trailing ;
        ('foo=bar;', {},
            Cookies(foo='bar'),
            Cookies(foo='bar'),
            ),
        ('A="BBB";', {},
            Cookies(A='BBB'),
            Cookies(A='BBB'),
            ),
        ('A=  "BBB"  ;', {},
            Cookies(A='BBB'),
            Cookies(A='BBB'),
            ),
        # empty value
        ("lang=; Expires=Sun, 06 Nov 1994 08:49:37 GMT", {},
            InvalidCookieError,
            Cookies(
                Cookie('lang', '',
                    expires=parse_date(
                        "Sun, 06 Nov 1994 08:49:37 GMT")))),
        # normal examples of varying complexity
        ("frob=varvels; Expires=Wed, 09 Jun 2021 10:18:14 GMT", {},
            InvalidCookieError,
            Cookies(
                Cookie('frob', 'varvels',
                    expires=parse_date(
                        "Wed, 09 Jun 2021 10:18:14 GMT"
                        )))),
        ("lang=en-US; Expires=Wed, 03 Jun 2019 10:18:14 GMT", {},
            InvalidCookieError,
            Cookies(
                Cookie('lang', 'en-US',
                    expires=parse_date(
                        "Wed, 03 Jun 2019 10:18:14 GMT"
                        )))),
        # easily interpretable as multiple request cookies!
        ("CID=39b4d9be4d42; Path=/; Domain=example.com", {},
            Cookies(CID="39b4d9be4d42", Path='/', Domain='example.com'),
            Cookies(Cookie('CID', '39b4d9be4d42', path='/',
                domain='example.com'))),
        ("lang=en-US; Path=/; Domain=example.com", {},
            Cookies(lang='en-US', Path='/', Domain='example.com'),
            Cookies(Cookie('lang', 'en-US',
                path='/', domain='example.com'))),
        ("foo=bar; path=/; expires=Mon, 04-Dec-2001 12:43:00 GMT", {},
            InvalidCookieError,
            Cookies(
                Cookie('foo', 'bar', path='/',
                    expires=parse_date("Mon, 04-Dec-2001 12:43:00 GMT")
                ))),
        ("SID=0fae49; Path=/; Secure; HttpOnly", {},
            InvalidCookieError,
            Cookies(Cookie('SID', '0fae49',
                path='/', secure=True, httponly=True))),
        ('TMID=DQAAXKEaeo_aYp; Domain=mail.nauk.com; '
            'Path=/accounts; Expires=Wed, 13-Jan-2021 22:23:01 GMT; '
            'Secure; HttpOnly', {},
            InvalidCookieError,
            Cookies(
                Cookie('TMID', 'DQAAXKEaeo_aYp',
                    domain='mail.nauk.com',
                    path='/accounts', secure=True, httponly=True,
                    expires=parse_date("Wed, 13-Jan-2021 22:23:01 GMT")
                    ))),
        ("test=some_value; expires=Sat, 01-Jan-2000 00:00:00 GMT; "
         "path=/;", {},
            InvalidCookieError,
            Cookies(
                Cookie('test', 'some_value', path='/',
                    expires=parse_date('Sat, 01 Jan 2000 00:00:00 GMT')
                ))),
        # From RFC 2109 - accept the lots-of-dquotes style but don't produce.
        ('Customer="WILE_E_COYOTE"; Version="1"; Path="/acme"; '
         'Part_Number="Rocket_Launcher_0001"', {},
            Cookies(Customer='WILE_E_COYOTE', Version='1', Path='/acme',
                Part_Number='Rocket_Launcher_0001'),
            Cookies(Cookie('Customer', 'WILE_E_COYOTE',
                    version=1, path='/acme'))),
        # However, we don't honor RFC 2109 type meta-attributes
        ('Cookie: $Version="1"; Customer="WILE_E_COYOTE"; $Path="/acme"', {},
            InvalidCookieError,
            InvalidCookieError),
        # degenerate Domain=. is common, so should be handled though invalid
        ("lu=Qg3OHJZLehYLjVgAqiZbZbzo; Expires=Tue, 15-Jan-2013 "
         "21:47:38 GMT; Path=/; Domain=.foo.com; HttpOnly", {},
            InvalidCookieError,
            Cookies(Cookie('lu', "Qg3OHJZLehYLjVgAqiZbZbzo",
                expires=parse_date('Tue, 15 Jan 2013 21:47:38 GMT'),
                path='/', domain='.foo.com', httponly=True,
                ))),
        ('ZQID=AYBEVnDKrdst; Domain=.nauk.com; Path=/; '
         'Expires=Wed, 13-Jan-2021 22:23:01 GMT; HttpOnly', {},
            InvalidCookieError,
            Cookies(Cookie('ZQID', "AYBEVnDKrdst",
                httponly=True, domain='.nauk.com', path='/',
                expires=parse_date('Wed, 13 Jan 2021 22:23:01 GMT'),
                ))),
        ("OMID=Ap4PQQEq; Domain=.nauk.com; Path=/; "
            'Expires=Wed, 13-Jan-2021 22:23:01 GMT; Secure; HttpOnly', {},
            InvalidCookieError,
            Cookies(Cookie('OMID', "Ap4PQQEq",
                path='/', domain='.nauk.com', secure=True, httponly=True,
                expires=parse_date('Wed, 13 Jan 2021 22:23:01 GMT')
                ))),
        # question mark in value
        ('foo="?foo"; Path=/', {},
            Cookies(foo='?foo', Path='/'),
            Cookies(Cookie('foo', '?foo', path='/'))),
        # unusual format for secure/httponly
        ("a=b; Secure=true; HttpOnly=true;", {},
            Cookies(a='b', Secure='true', HttpOnly='true'),
            Cookies(Cookie('a', 'b', secure=True, httponly=True))),
        # invalid per RFC to have spaces in value, but here they are
        # URL-encoded by default. Extend the mechanism if this is no good
        ('user=RJMmei IORqmD; expires=Wed, 3 Nov 2007 23:20:39 GMT; path=/',
            {},
            InvalidCookieError,
            Cookies(
                Cookie('user', 'RJMmei IORqmD', path='/',
                    expires=parse_date("Wed, 3 Nov 2007 23:20:39 GMT")))),
        # Most characters from 32 to \x31 + 1 should be allowed in values -
        # not including space/32, dquote/34, comma/44.
        ("x=!#$%&'()*+-./01", {},
            Cookies(x="!#$%&'()*+-./01"),
            Cookies(x="!#$%&'()*+-./01")),
        # don't crash when value wrapped with quotes
        # http://bugs.python.org/issue3924
        ('a=b; version="1"', {},
            Cookies(a='b', version='1'),
            Cookies(Cookie('a', 'b', version=1))),
        # cookie with name 'expires'. inadvisable, but valid.
        # http://bugs.python.org/issue1117339
        ('expires=foo', {},
            Cookies(expires='foo'),
            Cookies(expires='foo')),
        # http://bugs.python.org/issue8826
        # quick date parsing spot-check, see test_parse_date for a real workout
        ('foo=bar; expires=Fri, 31-Dec-2010 23:59:59 GMT', {},
            InvalidCookieError,
            Cookies(
                Cookie('foo', 'bar',
                expires=datetime(2010, 12, 31, 23, 59, 59)))),
        # allow VALID equals sign in values - not even an issue in RFC 6265 or
        # this module, but very helpful for base64 and always worth checking.
        # http://bugs.python.org/issue403473
        ('a=Zm9vIGJhcg==', {},
            Cookies(a='Zm9vIGJhcg=='),
            Cookies(a='Zm9vIGJhcg==')),
        ('blah="Foo=2"', {},
            Cookies(blah='Foo=2'),
            Cookies(blah='Foo=2')),
        # take the first cookie in request parsing.
        # (response parse ignores the second one as a bad attribute)
        # http://bugs.python.org/issue1375011
        # http://bugs.python.org/issue1372650
        # http://bugs.python.org/issue7504
        ('foo=33;foo=34', {},
            Cookies(foo='33'),
            Cookies(foo='33')),
        # Colons in names (invalid!), as used by some dumb old Java/PHP code
        # http://bugs.python.org/issue2988
        # http://bugs.python.org/issue472646
        # http://bugs.python.org/issue2193
        ('a:b=c', {},
            Cookies(
                Cookie('a:b', 'c')),
            Cookies(
                Cookie('a:b', 'c'))),
#       # http://bugs.python.org/issue991266
#       # This module doesn't do the backslash quoting so this would
#       # effectively require allowing all possible characters inside arbitrary
#       # attributes, which does not seem reasonable.
#       ('foo=bar; Comment="\342\230\243"', {},
#           Cookies(foo='bar', Comment='\342\230\243'),
#           Cookies(
#               Cookie('foo', 'bar', comment='\342\230\243')
#               )),
        ]


def _cheap_request_parse(arg1, arg2):
    """Really cheap parse like what client code often does, for
    testing request rendering (determining order-insensitively whether two
    cookies-as-text are equivalent). 'a=b; x=y' type format
    """
    def crumble(arg):
        "Break down string into pieces"
        pieces = [piece.strip('\r\n ;') for piece in re.split("(\r\n|;)", arg)]
        pieces = [piece for piece in pieces if piece and '=' in piece]
        pieces = [tuple(piece.split("=", 1)) for piece in pieces]
        pieces = [(name.strip(), value.strip('" ')) for name, value in pieces]
        # Keep the first one in front (can use set down the line);
        # the rest are sorted
        if len(pieces) > 1:
            pieces = [pieces[0]] + sorted(pieces[1:])
        return pieces

    def dedupe(pieces):
        "Eliminate duplicate pieces"
        deduped = {}
        for name, value in pieces:
            if name in deduped:
                continue
            deduped[name] = value
        return sorted(deduped.items(),
                key=pieces.index)

    return dedupe(crumble(arg1)), crumble(arg2)


def _cheap_response_parse(arg1, arg2):
    """Silly parser for 'name=value; attr=attrvalue' format,
    to test out response renders
    """
    def crumble(arg):
        "Break down string into pieces"
        lines = [line for line in arg if line]
        done = []
        for line in lines:
            clauses = [clause for clause in line.split(';')]
            import logging
            logging.error("clauses %r", clauses)
            name, value = re.split(" *= *", clauses[0], 1)
            value = unquote(value.strip(' "'))
            attrs = [re.split(" *= *", clause, 1) \
                    for clause in clauses[1:] if clause]
            attrs = [attr for attr in attrs \
                     if attr[0] in Cookie.attribute_names]
            attrs = [(k, v.strip(' "')) for k, v in attrs]
            done.append((name, value, tuple(attrs)))
        return done
    result1 = crumble([arg1])
    result2 = crumble(arg2)
    return result1, result2


def test_render_request():
    """Test the request renderer against HEADER_CASES.
    Perhaps a wider range of values is tested in TestCookies.test_init.
    """
    for case in HEADER_CASES:
        arg, kwargs, cookies, _ = case
        # can't reproduce examples which are supposed to throw parse errors
        if isinstance(cookies, type) and issubclass(cookies, Exception):
            continue
        rendered = cookies.render_request()
        expected, actual = _cheap_request_parse(arg, rendered)
        # we can only use set() here because requests aren't order sensitive.
        assert set(actual) == set(expected)


def test_render_response():
    """Test the response renderer against HEADER_CASES.
    Perhaps a wider range of values is tested in TestCookies.test_init.
    """
    def filter_attrs(items):
        "Filter out the items which are Cookie attributes"
        return [(name, value) for (name, value) in items \
                if name.lower() in Cookie.attribute_names]

    for case in HEADER_CASES:
        arg, kwargs, _, cookies = case
        # can't reproduce examples which are supposed to throw parse errors
        if isinstance(cookies, type) and issubclass(cookies, Exception):
            continue
        rendered = cookies.render_response()
        expected, actual = _cheap_response_parse(arg, rendered)
        expected, actual = set(expected), set(actual)
        assert actual == expected, \
                "failed: %s -> %s | %s != %s" % (arg, repr(cookies), actual,
                        expected)


def test_backslash_roundtrip():
    """Check that backslash in input or value stays backslash internally but
    goes out as %5C, and comes back in again as a backslash.
    """
    reference = Cookie('xx', '\\')
    assert len(reference.value) == 1
    reference_request = reference.render_request()
    reference_response = reference.render_response()
    assert '\\' not in reference_request
    assert '\\' not in reference_response
    assert '%5C' in reference_request
    assert '%5C' in reference_response

    # Parse from multiple entry points
    raw_cookie = r'xx="\"'
    parsed_cookies = [Cookie.from_string(raw_cookie),
              Cookies.from_request(raw_cookie)['xx'],
              Cookies.from_response(raw_cookie)['xx']]
    for parsed_cookie in parsed_cookies:
        assert parsed_cookie.name == reference.name
        assert parsed_cookie.value == reference.value
        # Renders should match exactly
        request = parsed_cookie.render_request()
        response = parsed_cookie.render_response()
        assert request == reference_request
        assert response == reference_response
        # Reparses should too
        rrequest = Cookies.from_request(request)['xx']
        rresponse = Cookies.from_response(response)['xx']
        assert rrequest.name == reference.name
        assert rrequest.value == reference.value
        assert rresponse.name == reference.name
        assert rresponse.value == reference.value


def _simple_test(function, case_dict):
    "Macro for making simple case-based tests for a function call"
    def actual_test():
        "Test generated by _simple_test"
        for arg, expected in case_dict.items():
            logging.info("case for %s: %s %s",
                          repr(function), repr(arg), repr(expected))
            result = function(arg)
            assert result == expected, \
                    "%s(%s) != %s, rather %s" % (
                            function.__name__,
                            repr(arg),
                            repr(expected),
                            repr(result))
    actual_test.cases = case_dict
    return actual_test

test_strip_spaces_and_quotes = _simple_test(strip_spaces_and_quotes, {
    '   ': '',
    '""': '',
    '"': '"',
    "''": "''",
    '   foo    ': 'foo',
    'foo    ': 'foo',
    '    foo': 'foo',
    '   ""  ': '',
    ' " "  ': ' ',
    '   "  ': '"',
    'foo bar': 'foo bar',
    '"foo bar': '"foo bar',
    'foo bar"': 'foo bar"',
    '"foo bar"': 'foo bar',
    '"dquoted"': 'dquoted',
    '   "dquoted"': 'dquoted',
    '"dquoted"     ': 'dquoted',
    '   "dquoted"     ': 'dquoted',
    })

test_parse_string = _simple_test(parse_string, {
    None: None,
    '': '',
    b'': '',
    })

test_parse_domain = _simple_test(parse_domain, {
    '  foo   ': 'foo',
    '"foo"': 'foo',
    '  "foo"  ': 'foo',
    '.foo': '.foo',
    })

test_parse_path = _simple_test(parse_path, {
    })


def test_render_date():
    "Test date render routine directly with raw datetime objects"
    # Date rendering is also exercised pretty well in test_parse_date.

    cases = {
        # Error for anything which is not known UTC/GMT
        datetime(2001, 10, 11, tzinfo=FixedOffsetTz(60 * 60)):
            AssertionError,
        # A couple of baseline tests
        datetime(1970, 1, 1, 0, 0, 0):
            'Thu, 01 Jan 1970 00:00:00 GMT',
        datetime(2007, 9, 2, 13, 59, 49):
            'Sun, 02 Sep 2007 13:59:49 GMT',
        # Don't produce 1-digit hour
        datetime(2007, 9, 2, 1, 59, 49):
            "Sun, 02 Sep 2007 01:59:49 GMT",
        # Don't produce 1-digit minute
        datetime(2007, 9, 2, 1, 1, 49):
            "Sun, 02 Sep 2007 01:01:49 GMT",
        # Don't produce 1-digit second
        datetime(2007, 9, 2, 1, 1, 2):
            "Sun, 02 Sep 2007 01:01:02 GMT",
        # Allow crazy past/future years for cookie delete/persist
        datetime(1900, 9, 2, 1, 1, 2):
            "Sun, 02 Sep 1900 01:01:02 GMT",
        datetime(3000, 9, 2, 1, 1, 2):
            "Tue, 02 Sep 3000 01:01:02 GMT"
        }

    for dt, expected in cases.items():
        if isinstance(expected, type) and issubclass(expected, Exception):
            try:
                render_date(dt)
            except expected:
                continue
            except Exception as exception:
                raise AssertionError("expected %s, got %s"
                        % (expected, exception))
            raise AssertionError("expected %s, got no exception"
                % (expected))
        else:
            assert render_date(dt) == expected


def test_encoding_assumptions(check_unicode=False):
    "Document and test assumptions underlying URL encoding scheme"
    # Use the RFC 6265 based character class to build a regexp matcher that
    # will tell us whether or not a character is okay to put in cookie values.
    cookie_value_re = re.compile("[%s]" % Definitions.COOKIE_OCTET)
    # Figure out which characters are okay. (unichr doesn't exist in Python 3,
    # in Python 2 it shouldn't be an issue)
    cookie_value_safe1 = set(chr(i) for i in range(0, 256) \
                            if cookie_value_re.match(chr(i)))
    cookie_value_safe2 = set(unichr(i) for i in range(0, 256) \
                            if cookie_value_re.match(unichr(i)))
    # These two are NOT the same on Python3
    assert cookie_value_safe1 == cookie_value_safe2
    # Now which of these are quoted by urllib.quote?
    # caveat: Python 2.6 crashes if chr(127) is passed to quote and safe="",
    # so explicitly set it to b"" to avoid the issue
    safe_but_quoted = set(c for c in cookie_value_safe1
                          if quote(c, safe=b"") != c)
    # Produce a set of characters to give to urllib.quote for the safe parm.
    dont_quote = "".join(sorted(safe_but_quoted))
    # Make sure it works (and that it works because of what we passed)
    for c in dont_quote:
        assert quote(c, safe="") != c
        assert quote(c, safe=dont_quote) == c

    # Make sure that the result of using dont_quote as the safe characters for
    # urllib.quote produces stuff which is safe as a cookie value, but not
    # different unless it has to be.
    for i in range(0, 255):
        original = chr(i)
        quoted = quote(original, safe=dont_quote)
        # If it is a valid value for a cookie, that quoting should leave it
        # alone.
        if cookie_value_re.match(original):
            assert original == quoted
        # If it isn't a valid value, then the quoted value should be valid.
        else:
            assert cookie_value_re.match(quoted)

    assert set(dont_quote) == set("!#$%&'()*+/:<=>?@[]^`{|}~")

    # From 128 on urllib.quote will not work on a unichr() return value.
    # We'll want to encode utf-8 values into ASCII, then do the quoting.
    # Verify that this is reversible.
    if check_unicode:
        for c in (unichr(i) for i in range(0, 1114112)):
            asc = c.encode('utf-8')
            quoted = quote(asc, safe=dont_quote)
            unquoted = unquote(asc)
            unicoded = unquoted.decode('utf-8')
            assert unicoded == c

    # Now do the same for extension-av.
    extension_av_re = re.compile("[%s]" % Definitions.EXTENSION_AV)
    extension_av_safe = set(chr(i) for i in range(0, 256) \
                            if extension_av_re.match(chr(i)))
    safe_but_quoted = set(c for c in extension_av_safe \
                          if quote(c, safe="") != c)
    dont_quote = "".join(sorted(safe_but_quoted))
    for c in dont_quote:
        assert quote(c, safe="") != c
        assert quote(c, safe=dont_quote) == c

    for i in range(0, 255):
        original = chr(i)
        quoted = quote(original, safe=dont_quote)
        if extension_av_re.match(original):
            assert original == quoted
        else:
            assert extension_av_re.match(quoted)

    assert set(dont_quote) == set(' !"#$%&\'()*+,/:<=>?@[\\]^`{|}~')


test_encode_cookie_value = _simple_test(encode_cookie_value,
    {
        None: None,
        ' ': '%20',
        # let through
        '!': '!',
        '#': '#',
        '$': '$',
        '%': '%',
        '&': '&',
        "'": "'",
        '(': '(',
        ')': ')',
        '*': '*',
        '+': '+',
        '/': '/',
        ':': ':',
        '<': '<',
        '=': '=',
        '>': '>',
        '?': '?',
        '@': '@',
        '[': '[',
        ']': ']',
        '^': '^',
        '`': '`',
        '{': '{',
        '|': '|',
        '}': '}',
        '~': '~',
        # not let through
        ' ': '%20',
        '"': '%22',
        ',': '%2C',
        '\\': '%5C',
        'crud,': 'crud%2C',
    })

test_encode_extension_av = _simple_test(encode_extension_av,
    {
        None: '',
        '': '',
        'foo': 'foo',
        # stuff this lets through that cookie-value does not
        ' ': ' ',
        '"': '"',
        ',': ',',
        '\\': '\\',
        'yo\\b': 'yo\\b',
    })

test_valid_value = _simple_test(valid_value,
    {
        None: False,
        '': True,
        'ಠ_ಠ': True,
        'μῆνιν ἄειδε θεὰ Πηληϊάδεω Ἀχιλῆος': True,
        '这事情得搞好啊': True,
        '宮崎　駿': True,
        'أم كلثوم': True,
        'ედუარდ შევარდნაძე': True,
        'Myötähäpeä': True,
        'Pedro Almodóvar': True,
#       b'': True,
#       b'ABCDEFGHIJKLMNOPQRSTUVWXYZ': True,
        'Pedro Almodóvar'.encode('utf-8'): False,
    })

test_valid_date = _simple_test(valid_date,
    {
        datetime(2011, 1, 1): True,
        datetime(2011, 1, 1, tzinfo=FixedOffsetTz(1000)): False,
        datetime(2011, 1, 1, tzinfo=FixedOffsetTz(0)): True,
    })

test_valid_domain = _simple_test(valid_domain,
    {
        '': False,
        ' ': False,
        '.': False,
        '..': False,
        '.foo': True,
        '"foo"': False,
        'foo': True,
    })

test_valid_path = _simple_test(valid_path,
    {
        '': False,
        ' ': False,
        '/': True,
        'a': False,
        '/a': True,
        '\x00': False,
        '/\x00': False,
    })


def test_many_pairs():
    """Simple 'lots of pairs' test
    """
    from_request = Cookies.from_request
    header = "a0=0"
    for i in range(1, 100):
        i_range = list(range(0, i))
        cookies = from_request(header)
        assert len(cookies) == i
        for j in i_range:
            key = 'a%d' % j
            assert cookies[key].value == str(j * 10)
            assert cookies[key].render_request() == \
                    "a%d=%d" % (j, j * 10)

        # same test, different entry point
        cookies = Cookies()
        cookies.parse_request(header)
        assert len(cookies) == i
        for j in i_range:
            key = 'a%d' % j
            assert cookies[key].value == str(j * 10)
            assert cookies[key].render_request() == \
                    "a%d=%d" % (j, j * 10)

        # Add another piece to the header
        header += "; a%d=%d" % (i, i * 10)


def test_parse_value():
    # this really just glues together strip_spaces_and_quotes
    # and parse_string, so reuse their test cases
    cases = {}
    cases.update(test_strip_spaces_and_quotes.cases)
    cases.update(test_parse_string.cases)
    for inp, expected in cases.items():
        print("case", inp, expected)
        # Test with spaces allowed
        obtained = parse_value(inp, allow_spaces=True)
        assert obtained == expected

        # Test with spaces disallowed, if it could do anything
        if (isinstance(inp, bytes) and ' ' in inp.decode('utf-8').strip()) \
        or (not isinstance(inp, bytes) and inp and ' ' in inp.strip()):
            try:
                obtained = parse_value(inp, allow_spaces=False)
            except AssertionError:
                pass
            else:
                raise AssertionError("parse_value(%s, allow_spaces=False) "
                                     "did not raise" % repr(inp))


def test_total_seconds():
    """This wrapper probably doesn't need testing so much, and it's not
    entirely trivial to fully exercise, but the coverage is nice to have
    """
    def basic_sanity(td_type):
        assert _total_seconds(td_type(seconds=1)) == 1
        assert _total_seconds(td_type(seconds=1, minutes=1)) == 1 + 60
        assert _total_seconds(td_type(seconds=1, minutes=1, hours=1)) == \
                1 + 60 + 60 * 60

    basic_sanity(timedelta)

    class FakeTimeDelta(object):
        def __init__(self, days=0, hours=0, minutes=0, seconds=0,
                     microseconds=0):
            self.days = days
            self.seconds = seconds + minutes * 60 + hours * 60 * 60
            self.microseconds = microseconds

    assert not hasattr(FakeTimeDelta, "total_seconds")
    basic_sanity(FakeTimeDelta)

    FakeTimeDelta.total_seconds = lambda: None.missing_attribute
    try:
        _total_seconds(None)
    except AttributeError as e:
        assert 'total_seconds' not in str(e)


def test_valid_value_bad_quoter():
    def bad_quote(s):
        return "Frogs"

    assert valid_value("eep", quote=bad_quote) == False
