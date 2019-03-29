# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re
from xml import sax
import six

from compare_locales.parser import DTDParser
from .base import Checker


class DTDChecker(Checker):
    """Tests to run on DTD files.

    Uses xml.sax for the heavy lifting of xml parsing.

    The code tries to parse until it doesn't find any unresolved entities
    anymore. If it finds one, it tries to grab the key, and adds an empty
    <!ENTITY key ""> definition to the header.

    Also checks for some CSS and number heuristics in the values.
    """
    pattern = re.compile(r'.*\.dtd$')
    needs_reference = True  # to cast a wider net for known entity references

    eref = re.compile('&(%s);' % DTDParser.Name)
    tmpl = b'''<!DOCTYPE elem [%s]>
<elem>%s</elem>
'''
    xmllist = set(('amp', 'lt', 'gt', 'apos', 'quot'))

    def __init__(self, extra_tests, locale=None):
        super(DTDChecker, self).__init__(extra_tests, locale=locale)
        self.processContent = False
        if self.extra_tests is not None and 'android-dtd' in self.extra_tests:
            self.processContent = True
        self.__known_entities = None

    def known_entities(self, refValue):
        if self.__known_entities is None and self.reference is not None:
            self.__known_entities = set()
            for ent in self.reference.values():
                self.__known_entities.update(
                    self.entities_for_value(ent.raw_val))
        return self.__known_entities if self.__known_entities is not None \
            else self.entities_for_value(refValue)

    def entities_for_value(self, value):
        reflist = set(m.group(1)
                      for m in self.eref.finditer(value))
        reflist -= self.xmllist
        return reflist

    # Setup for XML parser, with default and text-only content handler
    class TextContent(sax.handler.ContentHandler):
        textcontent = ''

        def characters(self, content):
            self.textcontent += content

    defaulthandler = sax.handler.ContentHandler()
    texthandler = TextContent()

    numPattern = r'([0-9]+|[0-9]*\.[0-9]+)'
    num = re.compile('^%s$' % numPattern)
    lengthPattern = '%s(em|px|ch|cm|in)' % numPattern
    length = re.compile('^%s$' % lengthPattern)
    spec = re.compile(r'((?:min\-)?(?:width|height))[ \t\r\n]*:[ \t\r\n]*%s' %
                      lengthPattern)
    style = re.compile(
        r'^%(spec)s[ \t\r\n]*(;[ \t\r\n]*%(spec)s[ \t\r\n]*)*;?$' %
        {'spec': spec.pattern})

    def check(self, refEnt, l10nEnt):
        """Try to parse the refvalue inside a dummy element, and keep
        track of entities that we need to define to make that work.

        Return a checker that offers just those entities.
        """
        refValue, l10nValue = refEnt.raw_val, l10nEnt.raw_val
        # find entities the refValue references,
        # reusing markup from DTDParser.
        reflist = self.known_entities(refValue)
        inContext = self.entities_for_value(refValue)
        entities = ''.join('<!ENTITY %s "">' % s for s in sorted(reflist))
        parser = sax.make_parser()
        parser.setFeature(sax.handler.feature_external_ges, False)

        parser.setContentHandler(self.defaulthandler)
        try:
            parser.parse(
                six.BytesIO(self.tmpl %
                            (entities.encode('utf-8'),
                             refValue.encode('utf-8'))))
            # also catch stray %
            parser.parse(
                six.BytesIO(self.tmpl %
                            ((refEnt.all + entities).encode('utf-8'),
                             b'&%s;' % refEnt.key.encode('utf-8'))))
        except sax.SAXParseException as e:
            e  # noqa
            yield ('warning',
                   (0, 0),
                   "can't parse en-US value", 'xmlparse')

        # find entities the l10nValue references,
        # reusing markup from DTDParser.
        l10nlist = self.entities_for_value(l10nValue)
        missing = sorted(l10nlist - reflist)
        _entities = entities + ''.join('<!ENTITY %s "">' % s for s in missing)
        if self.processContent:
            self.texthandler.textcontent = ''
            parser.setContentHandler(self.texthandler)
        try:
            parser.parse(six.BytesIO(self.tmpl % (_entities.encode('utf-8'),
                         l10nValue.encode('utf-8'))))
            # also catch stray %
            # if this fails, we need to substract the entity definition
            parser.setContentHandler(self.defaulthandler)
            parser.parse(
                six.BytesIO(self.tmpl %
                            ((l10nEnt.all + _entities).encode('utf-8'),
                             b'&%s;' % l10nEnt.key.encode('utf-8'))))
        except sax.SAXParseException as e:
            # xml parse error, yield error
            # sometimes, the error is reported on our fake closing
            # element, make that the end of the last line
            lnr = e.getLineNumber() - 1
            lines = l10nValue.splitlines()
            if lnr > len(lines):
                lnr = len(lines)
                col = len(lines[lnr-1])
            else:
                col = e.getColumnNumber()
                if lnr == 1:
                    # first line starts with <elem>, substract
                    col -= len("<elem>")
                elif lnr == 0:
                    col -= len("<!DOCTYPE elem [")  # first line is DOCTYPE
            yield ('error', (lnr, col), ' '.join(e.args), 'xmlparse')

        warntmpl = u'Referencing unknown entity `%s`'
        if reflist:
            if inContext:
                elsewhere = reflist - inContext
                warntmpl += ' (%s used in context' % \
                    ', '.join(sorted(inContext))
                if elsewhere:
                    warntmpl += ', %s known)' % ', '.join(sorted(elsewhere))
                else:
                    warntmpl += ')'
            else:
                warntmpl += ' (%s known)' % ', '.join(sorted(reflist))
        for key in missing:
            yield ('warning', (0, 0), warntmpl % key,
                   'xmlparse')
        if inContext and l10nlist and l10nlist - inContext - set(missing):
            mismatch = sorted(l10nlist - inContext - set(missing))
            for key in mismatch:
                yield ('warning', (0, 0),
                       'Entity %s referenced, but %s used in context' % (
                           key,
                           ', '.join(sorted(inContext))
                ), 'xmlparse')

        # Number check
        if self.num.match(refValue) and not self.num.match(l10nValue):
            yield ('warning', 0, 'reference is a number', 'number')
        # CSS checks
        # just a length, width="100em"
        if self.length.match(refValue) and not self.length.match(l10nValue):
            yield ('error', 0, 'reference is a CSS length', 'css')
        # real CSS spec, style="width:100px;"
        if self.style.match(refValue):
            if not self.style.match(l10nValue):
                yield ('error', 0, 'reference is a CSS spec', 'css')
            else:
                # warn if different properties or units
                refMap = dict((s, u) for s, _, u in
                              self.spec.findall(refValue))
                msgs = []
                for s, _, u in self.spec.findall(l10nValue):
                    if s not in refMap:
                        msgs.insert(0, '%s only in l10n' % s)
                        continue
                    else:
                        ru = refMap.pop(s)
                        if u != ru:
                            msgs.append("units for %s don't match "
                                        "(%s != %s)" % (s, u, ru))
                for s in six.iterkeys(refMap):
                    msgs.insert(0, '%s only in reference' % s)
                if msgs:
                    yield ('warning', 0, ', '.join(msgs), 'css')

        if self.extra_tests is not None and 'android-dtd' in self.extra_tests:
            for t in self.processAndroidContent(self.texthandler.textcontent):
                yield t

    quoted = re.compile("(?P<q>[\"']).*(?P=q)$")

    def unicode_escape(self, str):
        """Helper method to try to decode all unicode escapes in a string.

        This code uses the standard python decode for unicode-escape, but
        that's somewhat tricky, as its input needs to be ascii. To get to
        ascii, the unicode string gets converted to ascii with
        backslashreplace, i.e., all non-ascii unicode chars get unicode
        escaped. And then we try to roll all of that back.
        Now, when that hits an error, that's from the original string, and we
        need to search for the actual error position in the original string,
        as the backslashreplace code changes string positions quite badly.
        See also the last check in TestAndroid.test_android_dtd, with a
        lengthy chinese string.
        """
        val = str.encode('ascii', 'backslashreplace')
        try:
            val.decode('unicode-escape')
        except UnicodeDecodeError as e:
            args = list(e.args)
            badstring = args[1][args[2]:args[3]]
            i = len(args[1][:args[2]].decode('unicode-escape'))
            args[2] = i
            args[3] = i + len(badstring)
            raise UnicodeDecodeError(*args)

    def processAndroidContent(self, val):
        """Check for the string values that Android puts into an XML container.

        http://developer.android.com/guide/topics/resources/string-resource.html#FormattingAndStyling  # noqa

        Check for unicode escapes and unescaped quotes and apostrophes,
        if string's not quoted.
        """
        # first, try to decode unicode escapes
        try:
            self.unicode_escape(val)
        except UnicodeDecodeError as e:
            yield ('error', e.args[2], e.args[4], 'android')
        # check for unescaped single or double quotes.
        # first, see if the complete string is single or double quoted,
        # that changes the rules
        m = self.quoted.match(val)
        if m:
            q = m.group('q')
            offset = 0
            val = val[1:-1]  # strip quotes
        else:
            q = "[\"']"
            offset = -1
        stray_quot = re.compile(r"[\\\\]*(%s)" % q)

        for m in stray_quot.finditer(val):
            if len(m.group(0)) % 2:
                # found an unescaped single or double quote, which message?
                if m.group(1) == '"':
                    msg = "Quotes in Android DTDs need escaping with \\\" "\
                          "or \\u0022, or put string in apostrophes."
                else:
                    msg = "Apostrophes in Android DTDs need escaping with "\
                          "\\' or \\u0027, or use \u2019, or put string in "\
                          "quotes."
                yield ('error', m.end(0)+offset, msg, 'android')
