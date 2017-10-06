# -*- coding: utf-8 -*-
from env import MISSING_LOCALES
from cgi import parse_header
from datetime import datetime, time as time_
from babel import __version__ as VERSION
from babel.core import Locale
from babel._compat import number_types
from babel.dates import format_datetime
from babel.messages import pofile, Catalog
from babel.messages.catalog import _parse_datetime_header


class PatchedCatalog(Catalog):
    def _get_mime_headers(self):
        headers = []
        headers.append(('Project-Id-Version',
                        '%s %s' % (self.project, self.version)))
        headers.append(('Report-Msgid-Bugs-To', self.msgid_bugs_address))
        headers.append(('POT-Creation-Date',
                        format_datetime(self.creation_date, 'yyyy-MM-dd HH:mmZ',
                                        locale='en')))
        if isinstance(self.revision_date, (datetime, time_) + number_types):
            headers.append(('PO-Revision-Date',
                            format_datetime(self.revision_date,
                                            'yyyy-MM-dd HH:mmZ', locale='en')))
        else:
            headers.append(('PO-Revision-Date', self.revision_date))
        headers.append(('Last-Translator', self.last_translator))
        if self.locale is not None:
            headers.append(('Language', str(self.locale)))
        if (self.locale is not None) and ('LANGUAGE' in self.language_team):
            headers.append(('Language-Team',
                            self.language_team.replace('LANGUAGE',
                                                       str(self.locale))))
        else:
            headers.append(('Language-Team', self.language_team))
        if self.locale is not None:
            headers.append(('Plural-Forms', self.plural_forms))
        headers.append(('MIME-Version', '1.0'))
        headers.append(('Content-Type',
                        'text/plain; charset=%s' % self.charset))
        headers.append(('Content-Transfer-Encoding', '8bit'))
        headers.append(('Generated-By', 'Babel %s\n' % VERSION))
        return headers

    def _set_mime_headers(self, headers):
        for name, value in headers:
            name = name.lower()
            if name == 'project-id-version':
                parts = value.split(' ')
                self.project = u' '.join(parts[:-1])
                self.version = parts[-1]
            elif name == 'report-msgid-bugs-to':
                self.msgid_bugs_address = value
            elif name == 'last-translator':
                self.last_translator = value
            elif name == 'language':
                if value in MISSING_LOCALES:
                    self.locale = Locale.parse(MISSING_LOCALES[value]['plural_rule'])
                else:
                    self.locale = Locale.parse(value)
            elif name == 'language-team':
                self.language_team = value
            elif name == 'content-type':
                mimetype, params = parse_header(value)
                if 'charset' in params:
                    self.charset = params['charset'].lower()
            elif name == 'plural-forms':
                _, params = parse_header(' ;' + value)
                self._num_plurals = int(params.get('nplurals', 2))
                self._plural_expr = params.get('plural', '(n != 1)')
            elif name == 'pot-creation-date':
                self.creation_date = _parse_datetime_header(value)
            elif name == 'po-revision-date':
                # Keep the value if it's not the default one
                if 'YEAR' not in value:
                    self.revision_date = _parse_datetime_header(value)

    mime_headers = property(_get_mime_headers, _set_mime_headers)


def read_po(fileobj, locale=None, domain=None, ignore_obsolete=False, charset=None):
    catalog = PatchedCatalog(locale=locale, domain=domain, charset=charset)
    parser = pofile.PoFileParser(catalog, ignore_obsolete)
    parser.parse(fileobj)
    return catalog
