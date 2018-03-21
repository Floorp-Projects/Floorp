# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals, print_function

import buildconfig
import os
import shutil
import sys
import unittest
import mozpack.path as mozpath
from contextlib import contextmanager
from mozunit import main
from mozbuild.backend import get_backend_class
from mozbuild.backend.configenvironment import ConfigEnvironment
from mozbuild.backend.recursivemake import RecursiveMakeBackend
from mozbuild.backend.fastermake import FasterMakeBackend
from mozbuild.base import MozbuildObject
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader
from mozbuild.util import ensureParentDir
from mozpack.files import FileFinder
from tempfile import mkdtemp


BASE_SUBSTS = [
    ('PYTHON', mozpath.normsep(sys.executable)),
    ('MOZ_UI_LOCALE', 'en-US'),
]


class TestBuild(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop('MOZCONFIG', None)
        os.environ.pop('MOZ_OBJDIR', None)
        os.environ.pop('MOZ_PGO', None)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    @contextmanager
    def do_test_backend(self, *backends, **kwargs):
        topobjdir = mkdtemp()
        try:
            config = ConfigEnvironment(buildconfig.topsrcdir, topobjdir,
                                       **kwargs)
            reader = BuildReader(config)
            emitter = TreeMetadataEmitter(config)
            moz_build = mozpath.join(config.topsrcdir, 'test.mozbuild')
            definitions = list(emitter.emit(
                reader.read_mozbuild(moz_build, config)))
            for backend in backends:
                backend(config).consume(definitions)

            yield config
        except:
            raise
        finally:
            if not os.environ.get('MOZ_NO_CLEANUP'):
                shutil.rmtree(topobjdir)

    @contextmanager
    def line_handler(self):
        lines = []

        def handle_make_line(line):
            lines.append(line)

        try:
            yield handle_make_line
        except:
            print('\n'.join(lines))
            raise

        if os.environ.get('MOZ_VERBOSE_MAKE'):
            print('\n'.join(lines))

    def test_recursive_make(self):
        substs = list(BASE_SUBSTS)
        with self.do_test_backend(RecursiveMakeBackend,
                                  substs=substs) as config:
            build = MozbuildObject(config.topsrcdir, None, None,
                                   config.topobjdir)
            overrides = [
                'install_manifest_depends=',
                'MOZ_JAR_MAKER_FILE_FORMAT=flat',
                'TEST_MOZBUILD=1',
            ]
            with self.line_handler() as handle_make_line:
                build._run_make(directory=config.topobjdir, target=overrides,
                                silent=False, line_handler=handle_make_line)

            self.validate(config)

    def test_faster_recursive_make(self):
        substs = list(BASE_SUBSTS) + [
            ('BUILD_BACKENDS', 'FasterMake+RecursiveMake'),
        ]
        with self.do_test_backend(get_backend_class(
                'FasterMake+RecursiveMake'), substs=substs) as config:
            buildid = mozpath.join(config.topobjdir, 'config', 'buildid')
            ensureParentDir(buildid)
            with open(buildid, 'w') as fh:
                fh.write('20100101012345\n')

            build = MozbuildObject(config.topsrcdir, None, None,
                                   config.topobjdir)
            overrides = [
                'install_manifest_depends=',
                'MOZ_JAR_MAKER_FILE_FORMAT=flat',
                'TEST_MOZBUILD=1',
            ]
            with self.line_handler() as handle_make_line:
                build._run_make(directory=config.topobjdir, target=overrides,
                                silent=False, line_handler=handle_make_line)

            self.validate(config)

    def test_faster_make(self):
        substs = list(BASE_SUBSTS) + [
            ('MOZ_BUILD_APP', 'dummy_app'),
            ('MOZ_WIDGET_TOOLKIT', 'dummy_widget'),
        ]
        with self.do_test_backend(RecursiveMakeBackend, FasterMakeBackend,
                                  substs=substs) as config:
            buildid = mozpath.join(config.topobjdir, 'config', 'buildid')
            ensureParentDir(buildid)
            with open(buildid, 'w') as fh:
                fh.write('20100101012345\n')

            build = MozbuildObject(config.topsrcdir, None, None,
                                   config.topobjdir)
            overrides = [
                'TEST_MOZBUILD=1',
            ]
            with self.line_handler() as handle_make_line:
                build._run_make(directory=mozpath.join(config.topobjdir,
                                                       'faster'),
                                target=overrides, silent=False,
                                line_handler=handle_make_line)

            self.validate(config)

    def validate(self, config):
        self.maxDiff = None
        test_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 'data', 'build') + os.sep

        # We want unicode instances out of the files, because having plain str
        # makes assertEqual diff output in case of error extra verbose because
        # of the difference in type.
        result = {
            p: f.open().read().decode('utf-8')
            for p, f in FileFinder(mozpath.join(config.topobjdir, 'dist'))
        }
        self.assertTrue(len(result))
        self.assertEqual(result, {
            'bin/baz.ini': 'baz.ini: FOO is foo\n',
            'bin/child/bar.ini': 'bar.ini\n',
            'bin/child2/foo.css': 'foo.css: FOO is foo\n',
            'bin/child2/qux.ini': 'qux.ini: BAR is not defined\n',
            'bin/chrome.manifest':
                'manifest chrome/foo.manifest\n'
                'manifest components/components.manifest\n',
            'bin/chrome/foo.manifest':
                'content bar foo/child/\n'
                'content foo foo/\n'
                'override chrome://foo/bar.svg#hello '
                'chrome://bar/bar.svg#hello\n',
            'bin/chrome/foo/bar.js': 'bar.js\n',
            'bin/chrome/foo/child/baz.jsm':
                '//@line 2 "%sbaz.jsm"\nbaz.jsm: FOO is foo\n' % (test_path),
            'bin/chrome/foo/child/hoge.js':
                '//@line 2 "%sbar.js"\nbar.js: FOO is foo\n' % (test_path),
            'bin/chrome/foo/foo.css': 'foo.css: FOO is foo\n',
            'bin/chrome/foo/foo.js': 'foo.js\n',
            'bin/chrome/foo/qux.js': 'bar.js\n',
            'bin/components/bar.js':
                '//@line 2 "%sbar.js"\nbar.js: FOO is foo\n' % (test_path),
            'bin/components/components.manifest':
                'component {foo} foo.js\ncomponent {bar} bar.js\n',
            'bin/components/foo.js': 'foo.js\n',
            'bin/defaults/pref/prefs.js': 'prefs.js\n',
            'bin/foo.ini': 'foo.ini\n',
            'bin/modules/baz.jsm':
                '//@line 2 "%sbaz.jsm"\nbaz.jsm: FOO is foo\n' % (test_path),
            'bin/modules/child/bar.jsm': 'bar.jsm\n',
            'bin/modules/child2/qux.jsm':
                '//@line 4 "%squx.jsm"\nqux.jsm: BAR is not defined\n'
                % (test_path),
            'bin/modules/foo.jsm': 'foo.jsm\n',
            'bin/res/resource': 'resource\n',
            'bin/res/child/resource2': 'resource2\n',

            'bin/app/baz.ini': 'baz.ini: FOO is bar\n',
            'bin/app/child/bar.ini': 'bar.ini\n',
            'bin/app/child2/qux.ini': 'qux.ini: BAR is defined\n',
            'bin/app/chrome.manifest':
                'manifest chrome/foo.manifest\n'
                'manifest components/components.manifest\n',
            'bin/app/chrome/foo.manifest':
                'content bar foo/child/\n'
                'content foo foo/\n'
                'override chrome://foo/bar.svg#hello '
                'chrome://bar/bar.svg#hello\n',
            'bin/app/chrome/foo/bar.js': 'bar.js\n',
            'bin/app/chrome/foo/child/baz.jsm':
                '//@line 2 "%sbaz.jsm"\nbaz.jsm: FOO is bar\n' % (test_path),
            'bin/app/chrome/foo/child/hoge.js':
                '//@line 2 "%sbar.js"\nbar.js: FOO is bar\n' % (test_path),
            'bin/app/chrome/foo/foo.css': 'foo.css: FOO is bar\n',
            'bin/app/chrome/foo/foo.js': 'foo.js\n',
            'bin/app/chrome/foo/qux.js': 'bar.js\n',
            'bin/app/components/bar.js':
                '//@line 2 "%sbar.js"\nbar.js: FOO is bar\n' % (test_path),
            'bin/app/components/components.manifest':
                'component {foo} foo.js\ncomponent {bar} bar.js\n',
            'bin/app/components/foo.js': 'foo.js\n',
            'bin/app/defaults/preferences/prefs.js': 'prefs.js\n',
            'bin/app/foo.css': 'foo.css: FOO is bar\n',
            'bin/app/foo.ini': 'foo.ini\n',
            'bin/app/modules/baz.jsm':
                '//@line 2 "%sbaz.jsm"\nbaz.jsm: FOO is bar\n' % (test_path),
            'bin/app/modules/child/bar.jsm': 'bar.jsm\n',
            'bin/app/modules/child2/qux.jsm':
                '//@line 2 "%squx.jsm"\nqux.jsm: BAR is defined\n'
                % (test_path),
            'bin/app/modules/foo.jsm': 'foo.jsm\n',
        })

if __name__ == '__main__':
    main()
