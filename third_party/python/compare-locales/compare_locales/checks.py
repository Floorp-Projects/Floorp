# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
from collections import Counter
from difflib import SequenceMatcher
from xml import sax
try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO

from compare_locales.parser import DTDParser, PropertiesEntity


class Checker(object):
    '''Abstract class to implement checks per file type.
    '''
    pattern = None
    # if a check uses all reference entities, set this to True
    needs_reference = False

    @classmethod
    def use(cls, file):
        return cls.pattern.match(file.file)

    def __init__(self, extra_tests):
        self.extra_tests = extra_tests
        self.reference = None

    def check(self, refEnt, l10nEnt):
        '''Given the reference and localized Entities, performs checks.

        This is a generator yielding tuples of
        - "warning" or "error", depending on what should be reported,
        - tuple of line, column info for the error within the string
        - description string to be shown in the report
        '''
        if True:
            raise NotImplementedError("Need to subclass")
        yield ("error", (0, 0), "This is an example error", "example")

    def set_reference(self, reference):
        '''Set the reference entities.
        Only do this if self.needs_reference is True.
        '''
        self.reference = reference


class PrintfException(Exception):
    def __init__(self, msg, pos):
        self.pos = pos
        self.msg = msg


class PropertiesChecker(Checker):
    '''Tests to run on .properties files.
    '''
    pattern = re.compile('.*\.properties$')
    printf = re.compile(r'%(?P<good>%|'
                        r'(?:(?P<number>[1-9][0-9]*)\$)?'
                        r'(?P<width>\*|[0-9]+)?'
                        r'(?P<prec>\.(?:\*|[0-9]+)?)?'
                        r'(?P<spec>[duxXosScpfg]))?')

    def check(self, refEnt, l10nEnt):
        '''Test for the different variable formats.
        '''
        refValue, l10nValue = refEnt.val, l10nEnt.val
        refSpecs = None
        # check for PluralForm.jsm stuff, should have the docs in the
        # comment
        if (refEnt.pre_comment
                and 'Localization_and_Plurals' in refEnt.pre_comment.all):
            # For plurals, common variable pattern is #1. Try that.
            pats = set(int(m.group(1)) for m in re.finditer('#([0-9]+)',
                                                            refValue))
            if len(pats) == 0:
                return
            lpats = set(int(m.group(1)) for m in re.finditer('#([0-9]+)',
                                                             l10nValue))
            if pats - lpats:
                yield ('warning', 0, 'not all variables used in l10n',
                       'plural')
                return
            if lpats - pats:
                yield ('error', 0, 'unreplaced variables in l10n',
                       'plural')
                return
            return
        # check for lost escapes
        raw_val = l10nEnt.raw_val
        for m in PropertiesEntity.escape.finditer(raw_val):
            if m.group('single') and \
               m.group('single') not in PropertiesEntity.known_escapes:
                yield ('warning', m.start(),
                       'unknown escape sequence, \\' + m.group('single'),
                       'escape')
        try:
            refSpecs = self.getPrintfSpecs(refValue)
        except PrintfException:
            refSpecs = []
        if refSpecs:
            for t in self.checkPrintf(refSpecs, l10nValue):
                yield t
            return

    def checkPrintf(self, refSpecs, l10nValue):
        try:
            l10nSpecs = self.getPrintfSpecs(l10nValue)
        except PrintfException, e:
            yield ('error', e.pos, e.msg, 'printf')
            return
        if refSpecs != l10nSpecs:
            sm = SequenceMatcher()
            sm.set_seqs(refSpecs, l10nSpecs)
            msgs = []
            warn = None
            for action, i1, i2, j1, j2 in sm.get_opcodes():
                if action == 'equal':
                    continue
                if action == 'delete':
                    # missing argument in l10n
                    if i2 == len(refSpecs):
                        # trailing specs missing, that's just a warning
                        warn = ', '.join('trailing argument %d `%s` missing' %
                                         (i+1, refSpecs[i])
                                         for i in xrange(i1, i2))
                    else:
                        for i in xrange(i1, i2):
                            msgs.append('argument %d `%s` missing' %
                                        (i+1, refSpecs[i]))
                    continue
                if action == 'insert':
                    # obsolete argument in l10n
                    for i in xrange(j1, j2):
                        msgs.append('argument %d `%s` obsolete' %
                                    (i+1, l10nSpecs[i]))
                    continue
                if action == 'replace':
                    for i, j in zip(xrange(i1, i2), xrange(j1, j2)):
                        msgs.append('argument %d `%s` should be `%s`' %
                                    (j+1, l10nSpecs[j], refSpecs[i]))
            if msgs:
                yield ('error', 0, ', '.join(msgs), 'printf')
            if warn is not None:
                yield ('warning', 0, warn, 'printf')

    def getPrintfSpecs(self, val):
        hasNumber = False
        specs = []
        for m in self.printf.finditer(val):
            if m.group("good") is None:
                # found just a '%', signal an error
                raise PrintfException('Found single %', m.start())
            if m.group("good") == '%':
                # escaped %
                continue
            if ((hasNumber and m.group('number') is None) or
                    (not hasNumber and specs and
                     m.group('number') is not None)):
                # mixed style, numbered and not
                raise PrintfException('Mixed ordered and non-ordered args',
                                      m.start())
            hasNumber = m.group('number') is not None
            if hasNumber:
                pos = int(m.group('number')) - 1
                ls = len(specs)
                if pos >= ls:
                    # pad specs
                    nones = pos - ls
                    specs[ls:pos] = nones*[None]
                    specs.append(m.group('spec'))
                else:
                    specs[pos] = m.group('spec')
            else:
                specs.append(m.group('spec'))
        # check for missing args
        if hasNumber and not all(specs):
            raise PrintfException('Ordered argument missing', 0)
        return specs


class DTDChecker(Checker):
    """Tests to run on DTD files.

    Uses xml.sax for the heavy lifting of xml parsing.

    The code tries to parse until it doesn't find any unresolved entities
    anymore. If it finds one, it tries to grab the key, and adds an empty
    <!ENTITY key ""> definition to the header.

    Also checks for some CSS and number heuristics in the values.
    """
    pattern = re.compile('.*\.dtd$')
    needs_reference = True  # to cast a wider net for known entity references

    eref = re.compile('&(%s);' % DTDParser.Name)
    tmpl = '''<!DOCTYPE elem [%s]>
<elem>%s</elem>
'''
    xmllist = set(('amp', 'lt', 'gt', 'apos', 'quot'))

    def __init__(self, extra_tests):
        super(DTDChecker, self).__init__(extra_tests)
        self.processContent = False
        if self.extra_tests is not None and 'android-dtd' in self.extra_tests:
            self.processContent = True
        self.__known_entities = None

    def known_entities(self, refValue):
        if self.__known_entities is None and self.reference is not None:
            self.__known_entities = set()
            for ent in self.reference:
                self.__known_entities.update(
                    self.entities_for_value(ent.raw_val))
        return self.__known_entities if self.__known_entities is not None \
            else self.entities_for_value(refValue)

    def entities_for_value(self, value):
        reflist = set(m.group(1).encode('utf-8')
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
    spec = re.compile(r'((?:min\-)?(?:width|height))\s*:\s*%s' %
                      lengthPattern)
    style = re.compile(r'^%(spec)s\s*(;\s*%(spec)s\s*)*;?$' %
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
            parser.parse(StringIO(self.tmpl %
                                  (entities, refValue.encode('utf-8'))))
            # also catch stray %
            parser.parse(StringIO(self.tmpl %
                                  (refEnt.all.encode('utf-8') + entities,
                                   '&%s;' % refEnt.key.encode('utf-8'))))
        except sax.SAXParseException, e:
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
            parser.parse(StringIO(self.tmpl % (_entities,
                         l10nValue.encode('utf-8'))))
            # also catch stray %
            # if this fails, we need to substract the entity definition
            parser.setContentHandler(self.defaulthandler)
            parser.parse(StringIO(self.tmpl % (
                l10nEnt.all.encode('utf-8') + _entities,
                '&%s;' % l10nEnt.key.encode('utf-8'))))
        except sax.SAXParseException, e:
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
            yield ('warning', (0, 0), warntmpl % key.decode('utf-8'),
                   'xmlparse')
        if inContext and l10nlist and l10nlist - inContext - set(missing):
            mismatch = sorted(l10nlist - inContext - set(missing))
            for key in mismatch:
                yield ('warning', (0, 0),
                       'Entity %s referenced, but %s used in context' % (
                           key.decode('utf-8'),
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
                for s in refMap.iterkeys():
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
        except UnicodeDecodeError, e:
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
        except UnicodeDecodeError, e:
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
                    msg = u"Quotes in Android DTDs need escaping with \\\" "\
                          u"or \\u0022, or put string in apostrophes."
                else:
                    msg = u"Apostrophes in Android DTDs need escaping with "\
                          u"\\' or \\u0027, or use \u2019, or put string in "\
                          u"quotes."
                yield ('error', m.end(0)+offset, msg, 'android')


class FluentChecker(Checker):
    '''Tests to run on Fluent (FTL) files.
    '''
    pattern = re.compile('.*\.ftl')

    def check(self, refEnt, l10nEnt):
        ref_entry = refEnt.entry
        l10n_entry = l10nEnt.entry
        # verify that values match, either both have a value or none
        if ref_entry.value is not None and l10n_entry.value is None:
            yield ('error', 0, 'Missing value', 'fluent')
        if ref_entry.value is None and l10n_entry.value is not None:
            offset = l10n_entry.value.span.start - l10n_entry.span.start
            yield ('error', offset, 'Obsolete value', 'fluent')

        # verify that we're having the same set of attributes
        ref_attr_names = set((attr.id.name for attr in ref_entry.attributes))
        ref_pos = dict((attr.id.name, i)
                       for i, attr in enumerate(ref_entry.attributes))
        l10n_attr_counts = \
            Counter(attr.id.name for attr in l10n_entry.attributes)
        l10n_attr_names = set(l10n_attr_counts)
        l10n_pos = dict((attr.id.name, i)
                        for i, attr in enumerate(l10n_entry.attributes))
        # check for duplicate Attributes
        # only warn to not trigger a merge skip
        for attr_name, cnt in l10n_attr_counts.items():
            if cnt > 1:
                offset = (
                    l10n_entry.attributes[l10n_pos[attr_name]].span.start
                    - l10n_entry.span.start)
                yield (
                    'warning',
                    offset,
                    'Attribute "{}" occurs {} times'.format(
                        attr_name, cnt),
                    'fluent')

        missing_attr_names = sorted(ref_attr_names - l10n_attr_names,
                                    key=lambda k: ref_pos[k])
        for attr_name in missing_attr_names:
            yield ('error', 0, 'Missing attribute: ' + attr_name, 'fluent')

        obsolete_attr_names = sorted(l10n_attr_names - ref_attr_names,
                                     key=lambda k: l10n_pos[k])
        obsolete_attrs = [
            attr
            for attr in l10n_entry.attributes
            if attr.id.name in obsolete_attr_names
        ]
        for attr in obsolete_attrs:
            yield ('error', attr.span.start - l10n_entry.span.start,
                   'Obsolete attribute: ' + attr.id.name, 'fluent')


def getChecker(file, extra_tests=None):
    if PropertiesChecker.use(file):
        return PropertiesChecker(extra_tests)
    if DTDChecker.use(file):
        return DTDChecker(extra_tests)
    if FluentChecker.use(file):
        return FluentChecker(extra_tests)
    return None
