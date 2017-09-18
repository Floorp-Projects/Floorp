import unittest
import os
import tempfile
import shutil

from compare_locales import mozpath
from compare_locales.paths import EnumerateApp, ProjectFiles

MAIL_INI = '''\
[general]
depth = ../..
all = mail/locales/all-locales

[compare]
dirs = mail

[includes]
# non-central apps might want to use %(topsrcdir)s here, or other vars
# RFE: that needs to be supported by compare-locales, too, though
toolkit = mozilla/toolkit/locales/l10n.ini

[include_toolkit]
type = hg
mozilla = mozilla-central
repo = http://hg.mozilla.org/
l10n.ini = toolkit/locales/l10n.ini
'''


MAIL_ALL_LOCALES = '''af
de
fr
'''

MAIL_FILTER_PY = '''
def test(mod, path, entity = None):
    if mod == 'toolkit' and path == 'ignored_path':
        return 'ignore'
    return 'error'
'''

TOOLKIT_INI = '''[general]
depth = ../..

[compare]
dirs = toolkit
'''


class TestApp(unittest.TestCase):
    def setUp(self):
        self.stage = tempfile.mkdtemp()
        mail = mozpath.join(self.stage, 'comm', 'mail', 'locales')
        toolkit = mozpath.join(
            self.stage, 'comm', 'mozilla', 'toolkit', 'locales')
        l10n = mozpath.join(self.stage, 'l10n-central', 'de', 'toolkit')
        os.makedirs(mozpath.join(mail, 'en-US'))
        os.makedirs(mozpath.join(toolkit, 'en-US'))
        os.makedirs(l10n)
        with open(mozpath.join(mail, 'l10n.ini'), 'w') as f:
            f.write(MAIL_INI)
        with open(mozpath.join(mail, 'all-locales'), 'w') as f:
            f.write(MAIL_ALL_LOCALES)
        with open(mozpath.join(mail, 'filter.py'), 'w') as f:
            f.write(MAIL_FILTER_PY)
        with open(mozpath.join(toolkit, 'l10n.ini'), 'w') as f:
            f.write(TOOLKIT_INI)
        with open(mozpath.join(mail, 'en-US', 'mail.ftl'), 'w') as f:
            f.write('')
        with open(mozpath.join(toolkit, 'en-US', 'platform.ftl'), 'w') as f:
            f.write('')
        with open(mozpath.join(l10n, 'localized.ftl'), 'w') as f:
            f.write('')

    def tearDown(self):
        shutil.rmtree(self.stage)

    def test_app(self):
        'Test parsing a App'
        app = EnumerateApp(
            mozpath.join(self.stage, 'comm', 'mail', 'locales', 'l10n.ini'),
            mozpath.join(self.stage, 'l10n-central'))
        self.assertListEqual(app.locales, ['af', 'de', 'fr'])
        self.assertEqual(len(app.config.children), 1)
        projectconfig = app.asConfig()
        self.assertListEqual(projectconfig.locales, ['af', 'de', 'fr'])
        files = ProjectFiles('de', [projectconfig])
        files = list(files)
        self.assertEqual(len(files), 3)

        l10nfile, reffile, mergefile, test = files[0]
        self.assertListEqual(mozpath.split(l10nfile)[-3:],
                             ['de', 'mail', 'mail.ftl'])
        self.assertListEqual(mozpath.split(reffile)[-4:],
                             ['mail', 'locales', 'en-US', 'mail.ftl'])
        self.assertIsNone(mergefile)
        self.assertSetEqual(test, set())

        l10nfile, reffile, mergefile, test = files[1]
        self.assertListEqual(mozpath.split(l10nfile)[-3:],
                             ['de', 'toolkit', 'localized.ftl'])
        self.assertListEqual(
            mozpath.split(reffile)[-6:],
            ['comm', 'mozilla', 'toolkit',
             'locales', 'en-US', 'localized.ftl'])
        self.assertIsNone(mergefile)
        self.assertSetEqual(test, set())

        l10nfile, reffile, mergefile, test = files[2]
        self.assertListEqual(mozpath.split(l10nfile)[-3:],
                             ['de', 'toolkit', 'platform.ftl'])
        self.assertListEqual(
            mozpath.split(reffile)[-6:],
            ['comm', 'mozilla', 'toolkit', 'locales', 'en-US', 'platform.ftl'])
        self.assertIsNone(mergefile)
        self.assertSetEqual(test, set())
