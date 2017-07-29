# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import os
import sys
import textwrap
import unittest

from mozunit import (
    main,
    MockedOpen,
)

from mozbuild.configure import (
    ConfigureError,
    ConfigureSandbox,
)
from mozbuild.util import exec_
from mozpack import path as mozpath

from buildconfig import topsrcdir
from common import (
    ConfigureTestSandbox,
    ensure_exe_extension,
    fake_short_path,
)


class TestChecksConfigure(unittest.TestCase):
    def test_checking(self):
        out = StringIO()
        sandbox = ConfigureSandbox({}, stdout=out, stderr=out)
        base_dir = os.path.join(topsrcdir, 'build', 'moz.configure')
        sandbox.include_file(os.path.join(base_dir, 'checks.configure'))

        exec_(textwrap.dedent('''
            @checking('for a thing')
            def foo(value):
                return value
        '''), sandbox)

        foo = sandbox['foo']

        foo(True)
        self.assertEqual(out.getvalue(), 'checking for a thing... yes\n')

        out.truncate(0)
        foo(False)
        self.assertEqual(out.getvalue(), 'checking for a thing... no\n')

        out.truncate(0)
        foo(42)
        self.assertEqual(out.getvalue(), 'checking for a thing... 42\n')

        out.truncate(0)
        foo('foo')
        self.assertEqual(out.getvalue(), 'checking for a thing... foo\n')

        out.truncate(0)
        data = ['foo', 'bar']
        foo(data)
        self.assertEqual(out.getvalue(), 'checking for a thing... %r\n' % data)

        # When the function given to checking does nothing interesting, the
        # behavior is not altered
        exec_(textwrap.dedent('''
            @checking('for a thing', lambda x: x)
            def foo(value):
                return value
        '''), sandbox)

        foo = sandbox['foo']

        out.truncate(0)
        foo(True)
        self.assertEqual(out.getvalue(), 'checking for a thing... yes\n')

        out.truncate(0)
        foo(False)
        self.assertEqual(out.getvalue(), 'checking for a thing... no\n')

        out.truncate(0)
        foo(42)
        self.assertEqual(out.getvalue(), 'checking for a thing... 42\n')

        out.truncate(0)
        foo('foo')
        self.assertEqual(out.getvalue(), 'checking for a thing... foo\n')

        out.truncate(0)
        data = ['foo', 'bar']
        foo(data)
        self.assertEqual(out.getvalue(), 'checking for a thing... %r\n' % data)

        exec_(textwrap.dedent('''
            def munge(x):
                if not x:
                    return 'not found'
                if isinstance(x, (str, bool, int)):
                    return x
                return ' '.join(x)

            @checking('for a thing', munge)
            def foo(value):
                return value
        '''), sandbox)

        foo = sandbox['foo']

        out.truncate(0)
        foo(True)
        self.assertEqual(out.getvalue(), 'checking for a thing... yes\n')

        out.truncate(0)
        foo(False)
        self.assertEqual(out.getvalue(), 'checking for a thing... not found\n')

        out.truncate(0)
        foo(42)
        self.assertEqual(out.getvalue(), 'checking for a thing... 42\n')

        out.truncate(0)
        foo('foo')
        self.assertEqual(out.getvalue(), 'checking for a thing... foo\n')

        out.truncate(0)
        foo(['foo', 'bar'])
        self.assertEqual(out.getvalue(), 'checking for a thing... foo bar\n')

    KNOWN_A = ensure_exe_extension(mozpath.abspath('/usr/bin/known-a'))
    KNOWN_B = ensure_exe_extension(mozpath.abspath('/usr/local/bin/known-b'))
    KNOWN_C = ensure_exe_extension(mozpath.abspath('/home/user/bin/known c'))
    OTHER_A = ensure_exe_extension(mozpath.abspath('/lib/other/known-a'))

    def get_result(self, command='', args=[], environ={},
                   prog='/bin/configure', extra_paths=None,
                   includes=('util.configure', 'checks.configure')):
        config = {}
        out = StringIO()
        paths = {
            self.KNOWN_A: None,
            self.KNOWN_B: None,
            self.KNOWN_C: None,
        }
        if extra_paths:
            paths.update(extra_paths)
        environ = dict(environ)
        if 'PATH' not in environ:
            environ['PATH'] = os.pathsep.join(os.path.dirname(p) for p in paths)
        paths[self.OTHER_A] = None
        sandbox = ConfigureTestSandbox(paths, config, environ, [prog] + args,
                                       out, out)
        base_dir = os.path.join(topsrcdir, 'build', 'moz.configure')
        for f in includes:
            sandbox.include_file(os.path.join(base_dir, f))

        status = 0
        try:
            exec_(command, sandbox)
            sandbox.run()
        except SystemExit as e:
            status = e.code

        return config, out.getvalue(), status

    def test_check_prog(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ("known-a",))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_B})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_B)

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "known c"))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': fake_short_path(self.KNOWN_C)})
        self.assertEqual(out, "checking for foo... '%s'\n"
                              % fake_short_path(self.KNOWN_C))

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown",))')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo... not found
            DEBUG: foo: Trying unknown
            ERROR: Cannot find foo
        '''))

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"))')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo... not found
            DEBUG: foo: Trying unknown
            DEBUG: foo: Trying unknown-2
            DEBUG: foo: Trying 'unknown 3'
            ERROR: Cannot find foo
        '''))

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"), '
            'allow_missing=True)')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': ':'})
        self.assertEqual(out, 'checking for foo... not found\n')

    @unittest.skipIf(not sys.platform.startswith('win'), 'Windows-only test')
    def test_check_prog_exe(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=known-a.exe'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=%s' % os.path.splitext(self.KNOWN_A)[0]])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)


    def test_check_prog_with_args(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=%s' % self.KNOWN_A])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)

        path = self.KNOWN_B.replace('known-b', 'known-a')
        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=%s' % path])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo... not found
            DEBUG: foo: Trying %s
            ERROR: Cannot find foo
        ''') % path)

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown",))',
            ['FOO=known c'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': fake_short_path(self.KNOWN_C)})
        self.assertEqual(out, "checking for foo... '%s'\n"
                              % fake_short_path(self.KNOWN_C))

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"), '
            'allow_missing=True)', ['FOO=unknown'])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo... not found
            DEBUG: foo: Trying unknown
            ERROR: Cannot find foo
        '''))

    def test_check_prog_what(self):
        config, out, status = self.get_result(
            'check_prog("CC", ("known-a",), what="the target C compiler")')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_A})
        self.assertEqual(
            out, 'checking for the target C compiler... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(
            'check_prog("CC", ("unknown", "unknown-2", "unknown 3"),'
            '           what="the target C compiler")')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for the target C compiler... not found
            DEBUG: cc: Trying unknown
            DEBUG: cc: Trying unknown-2
            DEBUG: cc: Trying 'unknown 3'
            ERROR: Cannot find the target C compiler
        '''))

    def test_check_prog_input(self):
        config, out, status = self.get_result(textwrap.dedent('''
            option("--with-ccache", nargs=1, help="ccache")
            check_prog("CCACHE", ("known-a",), input="--with-ccache")
        '''), ['--with-ccache=known-b'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CCACHE': self.KNOWN_B})
        self.assertEqual(out, 'checking for ccache... %s\n' % self.KNOWN_B)

        script = textwrap.dedent('''
            option(env="CC", nargs=1, help="compiler")
            @depends("CC")
            def compiler(value):
                return value[0].split()[0] if value else None
            check_prog("CC", ("known-a",), input=compiler)
        ''')
        config, out, status = self.get_result(script)
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_A})
        self.assertEqual(out, 'checking for cc... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(script, ['CC=known-b'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_B})
        self.assertEqual(out, 'checking for cc... %s\n' % self.KNOWN_B)

        config, out, status = self.get_result(script, ['CC=known-b -m32'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_B})
        self.assertEqual(out, 'checking for cc... %s\n' % self.KNOWN_B)

    def test_check_prog_progs(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ())')
        self.assertEqual(status, 0)
        self.assertEqual(config, {})
        self.assertEqual(out, '')

        config, out, status = self.get_result(
            'check_prog("FOO", ())', ['FOO=known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': self.KNOWN_A})
        self.assertEqual(out, 'checking for foo... %s\n' % self.KNOWN_A)

        script = textwrap.dedent('''
            option(env="TARGET", nargs=1, default="linux", help="target")
            @depends("TARGET")
            def compiler(value):
                if value:
                    if value[0] == "linux":
                        return ("gcc", "clang")
                    if value[0] == "winnt":
                        return ("cl", "clang-cl")
            check_prog("CC", compiler)
        ''')
        config, out, status = self.get_result(script)
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for cc... not found
            DEBUG: cc: Trying gcc
            DEBUG: cc: Trying clang
            ERROR: Cannot find cc
        '''))

        config, out, status = self.get_result(script, ['TARGET=linux'])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for cc... not found
            DEBUG: cc: Trying gcc
            DEBUG: cc: Trying clang
            ERROR: Cannot find cc
        '''))

        config, out, status = self.get_result(script, ['TARGET=winnt'])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for cc... not found
            DEBUG: cc: Trying cl
            DEBUG: cc: Trying clang-cl
            ERROR: Cannot find cc
        '''))

        config, out, status = self.get_result(script, ['TARGET=none'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {})
        self.assertEqual(out, '')

        config, out, status = self.get_result(script, ['TARGET=winnt',
                                                       'CC=known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_A})
        self.assertEqual(out, 'checking for cc... %s\n' % self.KNOWN_A)

        config, out, status = self.get_result(script, ['TARGET=none',
                                                       'CC=known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': self.KNOWN_A})
        self.assertEqual(out, 'checking for cc... %s\n' % self.KNOWN_A)

    def test_check_prog_configure_error(self):
        with self.assertRaises(ConfigureError) as e:
            self.get_result('check_prog("FOO", "foo")')

        self.assertEqual(e.exception.message,
                         'progs must resolve to a list or tuple!')

        with self.assertRaises(ConfigureError) as e:
            self.get_result(
                'foo = depends(when=True)(lambda: ("a", "b"))\n'
                'check_prog("FOO", ("known-a",), input=foo)'
            )

        self.assertEqual(e.exception.message,
                         'input must resolve to a tuple or a list with a '
                         'single element, or a string')

        with self.assertRaises(ConfigureError) as e:
            self.get_result(
                'foo = depends(when=True)(lambda: {"a": "b"})\n'
                'check_prog("FOO", ("known-a",), input=foo)'
            )

        self.assertEqual(e.exception.message,
                         'input must resolve to a tuple or a list with a '
                         'single element, or a string')

    def test_check_prog_with_path(self):
        config, out, status = self.get_result('check_prog("A", ("known-a",), paths=["/some/path"])')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for a... not found
            DEBUG: a: Trying known-a
            ERROR: Cannot find a
        '''))

        config, out, status = self.get_result('check_prog("A", ("known-a",), paths=["%s"])' %
                                              os.path.dirname(self.OTHER_A))
        self.assertEqual(status, 0)
        self.assertEqual(config, {'A': self.OTHER_A})
        self.assertEqual(out, textwrap.dedent('''\
            checking for a... %s
        ''' % self.OTHER_A))

        dirs = map(mozpath.dirname, (self.OTHER_A, self.KNOWN_A))
        config, out, status = self.get_result(textwrap.dedent('''\
            check_prog("A", ("known-a",), paths=["%s"])
        ''' % os.pathsep.join(dirs)))
        self.assertEqual(status, 0)
        self.assertEqual(config, {'A': self.OTHER_A})
        self.assertEqual(out, textwrap.dedent('''\
            checking for a... %s
        ''' % self.OTHER_A))

        dirs = map(mozpath.dirname, (self.KNOWN_A, self.KNOWN_B))
        config, out, status = self.get_result(textwrap.dedent('''\
            check_prog("A", ("known-a",), paths=["%s", "%s"])
        ''' % (os.pathsep.join(dirs), self.OTHER_A)))
        self.assertEqual(status, 0)
        self.assertEqual(config, {'A': self.KNOWN_A})
        self.assertEqual(out, textwrap.dedent('''\
            checking for a... %s
        ''' % self.KNOWN_A))

        config, out, status = self.get_result('check_prog("A", ("known-a",), paths="%s")' %
                                              os.path.dirname(self.OTHER_A))

        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, textwrap.dedent('''\
            checking for a... 
            DEBUG: a: Trying known-a
            ERROR: Paths provided to find_program must be a list of strings, not %r
        ''' % mozpath.dirname(self.OTHER_A)))

    def test_java_tool_checks(self):
        includes = ('util.configure', 'checks.configure', 'java.configure')

        def mock_valid_javac(_, args):
            if len(args) == 1 and args[0] == '-version':
                return 0, '1.8', ''
            self.fail("Unexpected arguments to mock_valid_javac: %s" % args)

        # A valid set of tools in a standard location.
        java = mozpath.abspath('/usr/bin/java')
        javah = mozpath.abspath('/usr/bin/javah')
        javac = mozpath.abspath('/usr/bin/javac')
        jar = mozpath.abspath('/usr/bin/jar')
        jarsigner = mozpath.abspath('/usr/bin/jarsigner')
        keytool = mozpath.abspath('/usr/bin/keytool')

        paths = {
            java: None,
            javah: None,
            javac: mock_valid_javac,
            jar: None,
            jarsigner: None,
            keytool: None,
        }

        config, out, status = self.get_result(includes=includes, extra_paths=paths)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'JAVA': java,
            'JAVAH': javah,
            'JAVAC': javac,
            'JAR': jar,
            'JARSIGNER': jarsigner,
            'KEYTOOL': keytool,
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... %s
             checking for keytool... %s
             checking for javac... %s
             checking for javac version... 1.8
        ''' % (java, javah, jar, jarsigner, keytool, javac)))

        # An alternative valid set of tools referred to by JAVA_HOME.
        alt_java = mozpath.abspath('/usr/local/bin/java')
        alt_javah = mozpath.abspath('/usr/local/bin/javah')
        alt_javac = mozpath.abspath('/usr/local/bin/javac')
        alt_jar = mozpath.abspath('/usr/local/bin/jar')
        alt_jarsigner = mozpath.abspath('/usr/local/bin/jarsigner')
        alt_keytool = mozpath.abspath('/usr/local/bin/keytool')
        alt_java_home = mozpath.dirname(mozpath.dirname(alt_java))

        paths.update({
            alt_java: None,
            alt_javah: None,
            alt_javac: mock_valid_javac,
            alt_jar: None,
            alt_jarsigner: None,
            alt_keytool: None,
        })

        config, out, status = self.get_result(includes=includes,
                                              extra_paths=paths,
                                              environ={
                                                  'JAVA_HOME': alt_java_home,
                                                  'PATH': mozpath.dirname(java)
                                              })
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'JAVA': alt_java,
            'JAVAH': alt_javah,
            'JAVAC': alt_javac,
            'JAR': alt_jar,
            'JARSIGNER': alt_jarsigner,
            'KEYTOOL': alt_keytool,
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... %s
             checking for keytool... %s
             checking for javac... %s
             checking for javac version... 1.8
        ''' % (alt_java, alt_javah, alt_jar, alt_jarsigner,
               alt_keytool, alt_javac)))

        # We can use --with-java-bin-path instead of JAVA_HOME to similar
        # effect.
        config, out, status = self.get_result(
            args=['--with-java-bin-path=%s' % mozpath.dirname(alt_java)],
            includes=includes,
            extra_paths=paths,
            environ={
                'PATH': mozpath.dirname(java)
            })
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'JAVA': alt_java,
            'JAVAH': alt_javah,
            'JAVAC': alt_javac,
            'JAR': alt_jar,
            'JARSIGNER': alt_jarsigner,
            'KEYTOOL': alt_keytool,
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... %s
             checking for keytool... %s
             checking for javac... %s
             checking for javac version... 1.8
        ''' % (alt_java, alt_javah, alt_jar, alt_jarsigner,
               alt_keytool, alt_javac)))

        # If --with-java-bin-path and JAVA_HOME are both set,
        # --with-java-bin-path takes precedence.
        config, out, status = self.get_result(
            args=['--with-java-bin-path=%s' % mozpath.dirname(alt_java)],
            includes=includes,
            extra_paths=paths,
            environ={
                'PATH': mozpath.dirname(java),
                'JAVA_HOME': mozpath.dirname(mozpath.dirname(java)),
            })
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'JAVA': alt_java,
            'JAVAH': alt_javah,
            'JAVAC': alt_javac,
            'JAR': alt_jar,
            'JARSIGNER': alt_jarsigner,
            'KEYTOOL': alt_keytool,
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... %s
             checking for keytool... %s
             checking for javac... %s
             checking for javac version... 1.8
        ''' % (alt_java, alt_javah, alt_jar, alt_jarsigner,
               alt_keytool, alt_javac)))

        def mock_old_javac(_, args):
            if len(args) == 1 and args[0] == '-version':
                return 0, '1.6.9', ''
            self.fail("Unexpected arguments to mock_old_javac: %s" % args)

        # An old javac is fatal.
        paths[javac] = mock_old_javac
        config, out, status = self.get_result(includes=includes,
                                              extra_paths=paths,
                                              environ={
                                                  'PATH': mozpath.dirname(java)
                                              })
        self.assertEqual(status, 1)
        self.assertEqual(config, {
            'JAVA': java,
            'JAVAH': javah,
            'JAVAC': javac,
            'JAR': jar,
            'JARSIGNER': jarsigner,
            'KEYTOOL': keytool,
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... %s
             checking for keytool... %s
             checking for javac... %s
             checking for javac version... 
             ERROR: javac 1.8 or higher is required (found 1.6.9)
        ''' % (java, javah, jar, jarsigner, keytool, javac)))

        # Any missing tool is fatal when these checks run.
        del paths[jarsigner]
        config, out, status = self.get_result(includes=includes,
                                              extra_paths=paths,
                                              environ={
                                                  'PATH': mozpath.dirname(java)
                                              })
        self.assertEqual(status, 1)
        self.assertEqual(config, {
            'JAVA': java,
            'JAVAH': javah,
            'JAR': jar,
            'JARSIGNER': ':',
        })
        self.assertEqual(out, textwrap.dedent('''\
             checking for java... %s
             checking for javah... %s
             checking for jar... %s
             checking for jarsigner... not found
             ERROR: The program jarsigner was not found.  Set $JAVA_HOME to your Java SDK directory or use '--with-java-bin-path={java-bin-dir}'
        ''' % (java, javah, jar)))

    def test_pkg_check_modules(self):
        mock_pkg_config_version = '0.10.0'
        mock_pkg_config_path = mozpath.abspath('/usr/bin/pkg-config')

        def mock_pkg_config(_, args):
            if args[0:2] == ['--errors-to-stdout', '--print-errors']:
                assert len(args) == 3
                package = args[2]
                if package == 'unknown':
                    return (1, "Package unknown was not found in the pkg-config search path.\n"
                            "Perhaps you should add the directory containing `unknown.pc'\n"
                            "to the PKG_CONFIG_PATH environment variable\n"
                            "No package 'unknown' found", '')
                if package == 'valid':
                    return 0, '', ''
                if package == 'new > 1.1':
                    return 1, "Requested 'new > 1.1' but version of new is 1.1", ''
            if args[0] == '--cflags':
                assert len(args) == 2
                return 0, '-I/usr/include/%s' % args[1], ''
            if args[0] == '--libs':
                assert len(args) == 2
                return 0, '-l%s' % args[1], ''
            if args[0] == '--version':
                return 0, mock_pkg_config_version, ''
            self.fail("Unexpected arguments to mock_pkg_config: %s" % args)

        def get_result(cmd, args=[], extra_paths=None):
            return self.get_result(textwrap.dedent('''\
                option('--disable-compile-environment', help='compile env')
                include('%(topsrcdir)s/build/moz.configure/util.configure')
                include('%(topsrcdir)s/build/moz.configure/checks.configure')
                include('%(topsrcdir)s/build/moz.configure/pkg.configure')
            ''' % {'topsrcdir': topsrcdir}) + cmd, args=args, extra_paths=extra_paths,
                                                   includes=())

        extra_paths = {
            mock_pkg_config_path: mock_pkg_config,
        }
        includes = ('util.configure', 'checks.configure', 'pkg.configure')

        config, output, status = get_result("pkg_check_modules('MOZ_VALID', 'valid')")
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... not found
            ERROR: *** The pkg-config script could not be found. Make sure it is
            *** in your path, or set the PKG_CONFIG environment variable
            *** to the full path to pkg-config.
        '''))


        config, output, status = get_result("pkg_check_modules('MOZ_VALID', 'valid')",
                                            extra_paths=extra_paths)
        self.assertEqual(status, 0)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... %s
            checking for pkg-config version... %s
            checking for valid... yes
            checking MOZ_VALID_CFLAGS... -I/usr/include/valid
            checking MOZ_VALID_LIBS... -lvalid
        ''' % (mock_pkg_config_path, mock_pkg_config_version)))
        self.assertEqual(config, {
            'PKG_CONFIG': mock_pkg_config_path,
            'MOZ_VALID_CFLAGS': ('-I/usr/include/valid',),
            'MOZ_VALID_LIBS': ('-lvalid',),
        })

        config, output, status = get_result("pkg_check_modules('MOZ_UKNOWN', 'unknown')",
                                            extra_paths=extra_paths)
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... %s
            checking for pkg-config version... %s
            checking for unknown... no
            ERROR: Package unknown was not found in the pkg-config search path.
            ERROR: Perhaps you should add the directory containing `unknown.pc'
            ERROR: to the PKG_CONFIG_PATH environment variable
            ERROR: No package 'unknown' found
        ''' % (mock_pkg_config_path, mock_pkg_config_version)))
        self.assertEqual(config, {
            'PKG_CONFIG': mock_pkg_config_path,
        })

        config, output, status = get_result("pkg_check_modules('MOZ_NEW', 'new > 1.1')",
                                            extra_paths=extra_paths)
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... %s
            checking for pkg-config version... %s
            checking for new > 1.1... no
            ERROR: Requested 'new > 1.1' but version of new is 1.1
        ''' % (mock_pkg_config_path, mock_pkg_config_version)))
        self.assertEqual(config, {
            'PKG_CONFIG': mock_pkg_config_path,
        })

        # allow_missing makes missing packages non-fatal.
        cmd = textwrap.dedent('''\
        have_new_module = pkg_check_modules('MOZ_NEW', 'new > 1.1', allow_missing=True)
        @depends(have_new_module)
        def log_new_module_error(mod):
            if mod is not True:
                log.info('Module not found.')
        ''')

        config, output, status = get_result(cmd, extra_paths=extra_paths)
        self.assertEqual(status, 0)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... %s
            checking for pkg-config version... %s
            checking for new > 1.1... no
            WARNING: Requested 'new > 1.1' but version of new is 1.1
            Module not found.
        ''' % (mock_pkg_config_path, mock_pkg_config_version)))
        self.assertEqual(config, {
            'PKG_CONFIG': mock_pkg_config_path,
        })

        config, output, status = get_result(cmd,
                                            args=['--disable-compile-environment'],
                                            extra_paths=extra_paths)
        self.assertEqual(status, 0)
        self.assertEqual(output, 'Module not found.\n')
        self.assertEqual(config, {})

        def mock_old_pkg_config(_, args):
            if args[0] == '--version':
                return 0, '0.8.10', ''
            self.fail("Unexpected arguments to mock_old_pkg_config: %s" % args)

        extra_paths = {
            mock_pkg_config_path: mock_old_pkg_config,
        }

        config, output, status = get_result("pkg_check_modules('MOZ_VALID', 'valid')",
                                            extra_paths=extra_paths)
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for pkg_config... %s
            checking for pkg-config version... 0.8.10
            ERROR: *** Your version of pkg-config is too old. You need version 0.9.0 or newer.
        ''' % mock_pkg_config_path))

    def test_simple_keyfile(self):
        includes = ('util.configure', 'checks.configure', 'keyfiles.configure')

        config, output, status = self.get_result(
            "simple_keyfile('Mozilla API')", includes=includes)
        self.assertEqual(status, 0)
        self.assertEqual(output, textwrap.dedent('''\
            checking for the Mozilla API key... no
        '''))
        self.assertEqual(config, {
            'MOZ_MOZILLA_API_KEY': 'no-mozilla-api-key',
        })

        config, output, status = self.get_result(
            "simple_keyfile('Mozilla API')",
            args=['--with-mozilla-api-keyfile=/foo/bar/does/not/exist'],
            includes=includes)
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for the Mozilla API key... no
            ERROR: '/foo/bar/does/not/exist': No such file or directory.
        '''))
        self.assertEqual(config, {})

        with MockedOpen({'key': ''}):
            config, output, status = self.get_result(
                "simple_keyfile('Mozilla API')",
                args=['--with-mozilla-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 1)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Mozilla API key... no
                ERROR: 'key' is empty.
            '''))
            self.assertEqual(config, {})

        with MockedOpen({'key': 'fake-key\n'}):
            config, output, status = self.get_result(
                "simple_keyfile('Mozilla API')",
                args=['--with-mozilla-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Mozilla API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_MOZILLA_API_KEY': 'fake-key',
            })

        with MockedOpen({'default': 'default-key\n'}):
            config, output, status = self.get_result(
                "simple_keyfile('Mozilla API', default='default')",
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Mozilla API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_MOZILLA_API_KEY': 'default-key',
            })

        with MockedOpen({'default': 'default-key\n',
                         'key': 'fake-key\n'}):
            config, output, status = self.get_result(
                "simple_keyfile('Mozilla API', default='key')",
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Mozilla API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_MOZILLA_API_KEY': 'fake-key',
            })

    def test_id_and_secret_keyfile(self):
        includes = ('util.configure', 'checks.configure', 'keyfiles.configure')

        config, output, status = self.get_result(
            "id_and_secret_keyfile('Bing API')", includes=includes)
        self.assertEqual(status, 0)
        self.assertEqual(output, textwrap.dedent('''\
            checking for the Bing API key... no
        '''))
        self.assertEqual(config, {
            'MOZ_BING_API_CLIENTID': 'no-bing-api-clientid',
            'MOZ_BING_API_KEY': 'no-bing-api-key',
        })

        config, output, status = self.get_result(
            "id_and_secret_keyfile('Bing API')",
            args=['--with-bing-api-keyfile=/foo/bar/does/not/exist'],
            includes=includes)
        self.assertEqual(status, 1)
        self.assertEqual(output, textwrap.dedent('''\
            checking for the Bing API key... no
            ERROR: '/foo/bar/does/not/exist': No such file or directory.
        '''))
        self.assertEqual(config, {})

        with MockedOpen({'key': ''}):
            config, output, status = self.get_result(
                "id_and_secret_keyfile('Bing API')",
                args=['--with-bing-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 1)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Bing API key... no
                ERROR: 'key' is empty.
            '''))
            self.assertEqual(config, {})

        with MockedOpen({'key': 'fake-id fake-key\n'}):
            config, output, status = self.get_result(
                "id_and_secret_keyfile('Bing API')",
                args=['--with-bing-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Bing API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_BING_API_CLIENTID': 'fake-id',
                'MOZ_BING_API_KEY': 'fake-key',
            })

        with MockedOpen({'key': 'fake-key\n'}):
            config, output, status = self.get_result(
                "id_and_secret_keyfile('Bing API')",
                args=['--with-bing-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 1)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Bing API key... no
                ERROR: Bing API key file has an invalid format.
            '''))
            self.assertEqual(config, {})

        with MockedOpen({'default-key': 'default-id default-key\n'}):
            config, output, status = self.get_result(
                "id_and_secret_keyfile('Bing API', default='default-key')",
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Bing API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_BING_API_CLIENTID': 'default-id',
                'MOZ_BING_API_KEY': 'default-key',
            })

        with MockedOpen({'default-key': 'default-id default-key\n',
                         'key': 'fake-id fake-key\n'}):
            config, output, status = self.get_result(
                "id_and_secret_keyfile('Bing API', default='default-key')",
                args=['--with-bing-api-keyfile=key'],
                includes=includes)
            self.assertEqual(status, 0)
            self.assertEqual(output, textwrap.dedent('''\
                checking for the Bing API key... yes
            '''))
            self.assertEqual(config, {
                'MOZ_BING_API_CLIENTID': 'fake-id',
                'MOZ_BING_API_KEY': 'fake-key',
            })


if __name__ == '__main__':
    main()
