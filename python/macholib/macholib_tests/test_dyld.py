from macholib import dyld

import sys
import os
import functools

if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest

class DyldPatcher (object):
    def __init__(self):
        self.calls = []
        self.patched = {}

    def clear_calls(self):
        self.calls = []

    def cleanup(self):
        for name in self.patched:
            setattr(dyld, name, self.patched[name])

    def log_calls(self, name):
        if name in self.patched:
            return

        self.patched[name] = getattr(dyld, name)


        @functools.wraps(self.patched[name])
        def wrapper(*args, **kwds):
            self.calls.append((name, args, kwds))
            return self.patched[name](*args, **kwds)

        setattr(dyld, name, wrapper)


class TestDyld (unittest.TestCase):
    if not hasattr(unittest.TestCase, 'assertIsInstance'):
        def assertIsInstance(self, value, types, message=None):
            self.assertTrue(isinstance(value, types),
                message or "%r is not an instance of %r"%(value, types))

    def setUp(self):
        self._environ = os.environ
        os.environ = dict([(k, os.environ[k]) for k in os.environ if 'DYLD' not in k])
        self._dyld_env = dyld._dyld_env
        self._dyld_image_suffix = dyld.dyld_image_suffix

    def tearDown(self):
        dyld._dyld_env = self._dyld_env
        dyld.dyld_image_suffix = self._dyld_image_suffix
        os.environ = self._environ

    if sys.version_info[0] == 2:
        def test_ensure_utf8(self):
            self.assertEqual(dyld._ensure_utf8("hello"), "hello")
            self.assertEqual(dyld._ensure_utf8("hello".decode('utf-8')), "hello")
            self.assertEqual(dyld._ensure_utf8(None), None)

    else:
        def test_ensure_utf8(self):
            self.assertEqual(dyld._ensure_utf8("hello"), "hello")
            self.assertEqual(dyld._ensure_utf8(None), None)
            self.assertRaises(ValueError, dyld._ensure_utf8, b"hello")

    def test__dyld_env(self):
        new = os.environ

        self.assertEqual(dyld._dyld_env(None, 'DYLD_FOO'), [])
        self.assertEqual(dyld._dyld_env({'DYLD_FOO':'bar'}, 'DYLD_FOO'), ['bar'])
        self.assertEqual(dyld._dyld_env({'DYLD_FOO':'bar:baz'}, 'DYLD_FOO'), ['bar', 'baz'])
        self.assertEqual(dyld._dyld_env({}, 'DYLD_FOO'), [])
        self.assertEqual(dyld._dyld_env({'DYLD_FOO':''}, 'DYLD_FOO'), [])
        os.environ['DYLD_FOO'] = 'foobar'
        self.assertEqual(dyld._dyld_env(None, 'DYLD_FOO'), ['foobar'])
        os.environ['DYLD_FOO'] = 'foobar:nowhere'
        self.assertEqual(dyld._dyld_env(None, 'DYLD_FOO'), ['foobar', 'nowhere'])
        self.assertEqual(dyld._dyld_env({'DYLD_FOO':'bar'}, 'DYLD_FOO'), ['bar'])
        self.assertEqual(dyld._dyld_env({}, 'DYLD_FOO'), [])


        self.assertEqual(dyld.dyld_image_suffix(), None)
        self.assertEqual(dyld.dyld_image_suffix(None), None)
        self.assertEqual(dyld.dyld_image_suffix({'DYLD_IMAGE_SUFFIX':'bar'}), 'bar')
        os.environ['DYLD_IMAGE_SUFFIX'] = 'foobar'
        self.assertEqual(dyld.dyld_image_suffix(), 'foobar')
        self.assertEqual(dyld.dyld_image_suffix(None), 'foobar')

    def test_dyld_helpers(self):
        record = []
        def fake__dyld_env(env, key):
            record.append((env, key))
            return ['hello']

        dyld._dyld_env = fake__dyld_env
        self.assertEqual(dyld.dyld_framework_path(), ['hello'])
        self.assertEqual(dyld.dyld_framework_path({}), ['hello'])

        self.assertEqual(dyld.dyld_library_path(), ['hello'])
        self.assertEqual(dyld.dyld_library_path({}), ['hello'])

        self.assertEqual(dyld.dyld_fallback_framework_path(), ['hello'])
        self.assertEqual(dyld.dyld_fallback_framework_path({}), ['hello'])

        self.assertEqual(dyld.dyld_fallback_library_path(), ['hello'])
        self.assertEqual(dyld.dyld_fallback_library_path({}), ['hello'])

        self.assertEqual(record, [
            (None, 'DYLD_FRAMEWORK_PATH'),
            ({}, 'DYLD_FRAMEWORK_PATH'),
            (None, 'DYLD_LIBRARY_PATH'),
            ({}, 'DYLD_LIBRARY_PATH'),
            (None, 'DYLD_FALLBACK_FRAMEWORK_PATH'),
            ({}, 'DYLD_FALLBACK_FRAMEWORK_PATH'),
            (None, 'DYLD_FALLBACK_LIBRARY_PATH'),
            ({}, 'DYLD_FALLBACK_LIBRARY_PATH'),
        ])

    def test_dyld_suffix_search(self):
        envs = [object()]
        def fake_suffix(env):
            envs[0] = env
            return None
        dyld.dyld_image_suffix = fake_suffix

        iterator = [
            '/usr/lib/foo',
            '/usr/lib/foo.dylib',
        ]
        result = dyld.dyld_image_suffix_search(iter(iterator))
        self.assertEqual(list(result), iterator)
        self.assertEqual(envs[0], None)

        result = dyld.dyld_image_suffix_search(iter(iterator), {})
        self.assertEqual(list(result), iterator)
        self.assertEqual(envs[0], {})

        envs = [object()]
        def fake_suffix(env):
            envs[0] = env
            return '_profile'
        dyld.dyld_image_suffix = fake_suffix

        iterator = [
            '/usr/lib/foo',
            '/usr/lib/foo.dylib',
        ]
        result = dyld.dyld_image_suffix_search(iter(iterator))
        self.assertEqual(list(result), [
                '/usr/lib/foo_profile',
                '/usr/lib/foo',
                '/usr/lib/foo_profile.dylib',
                '/usr/lib/foo.dylib',
            ])
        self.assertEqual(envs[0], None)

        result = dyld.dyld_image_suffix_search(iter(iterator), {})
        self.assertEqual(list(result), [
                '/usr/lib/foo_profile',
                '/usr/lib/foo',
                '/usr/lib/foo_profile.dylib',
                '/usr/lib/foo.dylib',
            ])
        self.assertEqual(envs[0], {})

    def test_override_search(self):
        os.environ['DYLD_FRAMEWORK_PATH'] = ''
        os.environ['DYLD_LIBRARY_PATH'] = ''

        self.assertEqual(
                list(dyld.dyld_override_search("foo.dyld", None)), [])
        self.assertEqual(
                list(dyld.dyld_override_search("/usr/lib/libfoo.dyld", None)), [])
        self.assertEqual(
                list(dyld.dyld_override_search("/Library/Frameworks/Python.framework/Versions/Current/Python", None)), [])


        os.environ['DYLD_FRAMEWORK_PATH'] = '/Foo/Frameworks:/Bar/Frameworks'
        os.environ['DYLD_LIBRARY_PATH'] = ''
        self.assertEqual(
                list(dyld.dyld_override_search("foo.dyld", None)), [])
        self.assertEqual(
                list(dyld.dyld_override_search("/usr/lib/libfoo.dyld", None)), [])
        self.assertEqual(
                list(dyld.dyld_override_search("/Library/Frameworks/Python.framework/Versions/Current/Python", None)), [
                    '/Foo/Frameworks/Python.framework/Versions/Current/Python',
                    '/Bar/Frameworks/Python.framework/Versions/Current/Python',
                ])

        os.environ['DYLD_FRAMEWORK_PATH'] = ''
        os.environ['DYLD_LIBRARY_PATH'] = '/local/lib:/remote/lib'
        self.assertEqual(
                list(dyld.dyld_override_search("foo.dyld", None)), [
                        '/local/lib/foo.dyld',
                        '/remote/lib/foo.dyld',
                    ])
        self.assertEqual(
                list(dyld.dyld_override_search("/usr/lib/libfoo.dyld", None)), [
                        '/local/lib/libfoo.dyld',
                        '/remote/lib/libfoo.dyld',
                    ])
        self.assertEqual(
                list(dyld.dyld_override_search("/Library/Frameworks/Python.framework/Versions/Current/Python", None)), [
                        '/local/lib/Python',
                        '/remote/lib/Python',
                    ])

        os.environ['DYLD_FRAMEWORK_PATH'] = '/Foo/Frameworks:/Bar/Frameworks'
        os.environ['DYLD_LIBRARY_PATH'] = '/local/lib:/remote/lib'
        self.assertEqual(
                list(dyld.dyld_override_search("foo.dyld", None)), [
                        '/local/lib/foo.dyld',
                        '/remote/lib/foo.dyld',
                    ])
        self.assertEqual(
                list(dyld.dyld_override_search("/usr/lib/libfoo.dyld", None)), [
                        '/local/lib/libfoo.dyld',
                        '/remote/lib/libfoo.dyld',
                    ])
        self.assertEqual(
                list(dyld.dyld_override_search("/Library/Frameworks/Python.framework/Versions/Current/Python", None)), [
                    '/Foo/Frameworks/Python.framework/Versions/Current/Python',
                    '/Bar/Frameworks/Python.framework/Versions/Current/Python',
                    '/local/lib/Python',
                    '/remote/lib/Python',
                ])

    def test_executable_path_search(self):
        self.assertEqual(list(dyld.dyld_executable_path_search("/usr/lib/foo.dyld", "/usr/bin")), [])
        self.assertEqual(
                list(dyld.dyld_executable_path_search("@executable_path/foo.dyld", "/usr/bin")),
                ['/usr/bin/foo.dyld'])
        self.assertEqual(
                list(dyld.dyld_executable_path_search("@executable_path/../../lib/foo.dyld", "/usr/bin")),
                ['/usr/bin/../../lib/foo.dyld'])

    def test_default_search(self):
        self.assertEqual(
                list(dyld.dyld_default_search('/usr/lib/mylib.dylib', None)), [
                    '/usr/lib/mylib.dylib',
                    os.path.join(os.path.expanduser('~/lib'), 'mylib.dylib'),
                    '/usr/local/lib/mylib.dylib',
                    '/lib/mylib.dylib',
                    '/usr/lib/mylib.dylib',

                ])

        self.assertEqual(
                list(dyld.dyld_default_search('/Library/Frameworks/Python.framework/Versions/2.7/Python', None)), [
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    os.path.join(os.path.expanduser('~/Library/Frameworks'), 'Python.framework/Versions/2.7/Python'),
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/Network/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/System/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    os.path.join(os.path.expanduser('~/lib'), 'Python'),
                    '/usr/local/lib/Python',
                    '/lib/Python',
                    '/usr/lib/Python',
                ])




        os.environ['DYLD_FALLBACK_LIBRARY_PATH'] = '/local/lib:/network/lib'
        os.environ['DYLD_FALLBACK_FRAMEWORK_PATH'] = ''

        self.assertEqual(
                list(dyld.dyld_default_search('/usr/lib/mylib.dylib', None)), [
                    '/usr/lib/mylib.dylib',
                    '/local/lib/mylib.dylib',
                    '/network/lib/mylib.dylib',
                ])


        self.assertEqual(
                list(dyld.dyld_default_search('/Library/Frameworks/Python.framework/Versions/2.7/Python', None)), [
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    os.path.join(os.path.expanduser('~/Library/Frameworks'), 'Python.framework/Versions/2.7/Python'),
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/Network/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/System/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/local/lib/Python',
                    '/network/lib/Python',
                ])


        os.environ['DYLD_FALLBACK_LIBRARY_PATH'] = ''
        os.environ['DYLD_FALLBACK_FRAMEWORK_PATH'] = '/MyFrameworks:/OtherFrameworks'


        self.assertEqual(
                list(dyld.dyld_default_search('/usr/lib/mylib.dylib', None)), [
                    '/usr/lib/mylib.dylib',
                    os.path.join(os.path.expanduser('~/lib'), 'mylib.dylib'),
                    '/usr/local/lib/mylib.dylib',
                    '/lib/mylib.dylib',
                    '/usr/lib/mylib.dylib',

                ])

        self.assertEqual(
                list(dyld.dyld_default_search('/Library/Frameworks/Python.framework/Versions/2.7/Python', None)), [
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/MyFrameworks/Python.framework/Versions/2.7/Python',
                    '/OtherFrameworks/Python.framework/Versions/2.7/Python',
                    os.path.join(os.path.expanduser('~/lib'), 'Python'),
                    '/usr/local/lib/Python',
                    '/lib/Python',
                    '/usr/lib/Python',
                ])

        os.environ['DYLD_FALLBACK_LIBRARY_PATH'] = '/local/lib:/network/lib'
        os.environ['DYLD_FALLBACK_FRAMEWORK_PATH'] = '/MyFrameworks:/OtherFrameworks'


        self.assertEqual(
                list(dyld.dyld_default_search('/usr/lib/mylib.dylib', None)), [
                    '/usr/lib/mylib.dylib',
                    '/local/lib/mylib.dylib',
                    '/network/lib/mylib.dylib',

                ])

        self.assertEqual(
                list(dyld.dyld_default_search('/Library/Frameworks/Python.framework/Versions/2.7/Python', None)), [
                    '/Library/Frameworks/Python.framework/Versions/2.7/Python',
                    '/MyFrameworks/Python.framework/Versions/2.7/Python',
                    '/OtherFrameworks/Python.framework/Versions/2.7/Python',
                    '/local/lib/Python',
                    '/network/lib/Python',
                ])

    def test_dyld_find(self):
        result = dyld.dyld_find('/usr/lib/libSystem.dylib')
        self.assertEqual(result, '/usr/lib/libSystem.dylib')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        result = dyld.dyld_find(b'/usr/lib/libSystem.dylib'.decode('ascii'))
        self.assertEqual(result, '/usr/lib/libSystem.dylib')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        patcher = DyldPatcher()
        try:
            patcher.log_calls('dyld_image_suffix_search')
            patcher.log_calls('dyld_override_search')
            patcher.log_calls('dyld_executable_path_search')
            patcher.log_calls('dyld_default_search')

            result = dyld.dyld_find('/usr/lib/libSystem.dylib')
            self.assertEqual(patcher.calls[:-1], [
                ('dyld_override_search', ('/usr/lib/libSystem.dylib', None), {}),
                ('dyld_executable_path_search', ('/usr/lib/libSystem.dylib', None), {}),
                ('dyld_default_search', ('/usr/lib/libSystem.dylib', None), {}),
            ])
            self.assertEqual(patcher.calls[-1][0], 'dyld_image_suffix_search')
            patcher.clear_calls()

            result = dyld.dyld_find('/usr/lib/libSystem.dylib', env=None)
            self.assertEqual(patcher.calls[:-1], [
                ('dyld_override_search', ('/usr/lib/libSystem.dylib', None), {}),
                ('dyld_executable_path_search', ('/usr/lib/libSystem.dylib', None), {}),
                ('dyld_default_search', ('/usr/lib/libSystem.dylib', None), {}),
            ])
            self.assertEqual(patcher.calls[-1][0], 'dyld_image_suffix_search')
            patcher.clear_calls()

            result = dyld.dyld_find('/usr/lib/libSystem.dylib', env={})
            self.assertEqual(patcher.calls[:-1], [
                ('dyld_override_search', ('/usr/lib/libSystem.dylib', {}), {}),
                ('dyld_executable_path_search', ('/usr/lib/libSystem.dylib', None), {}),
                ('dyld_default_search', ('/usr/lib/libSystem.dylib', {}), {}),
            ])
            self.assertEqual(patcher.calls[-1][0], 'dyld_image_suffix_search')
            patcher.clear_calls()

            result = dyld.dyld_find('/usr/lib/libSystem.dylib', executable_path="/opt/py2app/bin", env={})
            self.assertEqual(patcher.calls[:-1], [
                ('dyld_override_search', ('/usr/lib/libSystem.dylib', {}), {}),
                ('dyld_executable_path_search', ('/usr/lib/libSystem.dylib', "/opt/py2app/bin"), {}),
                ('dyld_default_search', ('/usr/lib/libSystem.dylib', {}), {}),
            ])
            self.assertEqual(patcher.calls[-1][0], 'dyld_image_suffix_search')
            patcher.clear_calls()

        finally:
            patcher.cleanup()

    def test_framework_find(self):
        result = dyld.framework_find('/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa')
        self.assertEqual(result, '/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        result = dyld.framework_find(b'/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa'.decode('latin1'))
        self.assertEqual(result, '/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        result = dyld.framework_find('Cocoa.framework')
        self.assertEqual(result, '/System/Library/Frameworks/Cocoa.framework/Cocoa')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        result = dyld.framework_find('Cocoa')
        self.assertEqual(result, '/System/Library/Frameworks/Cocoa.framework/Cocoa')
        self.assertIsInstance(result, str) # bytes on 2.x, unicode on 3.x

        patcher = DyldPatcher()
        try:
            patcher.log_calls('dyld_find')

            result = dyld.framework_find('/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa')
            self.assertEqual(patcher.calls, [
                ('dyld_find', ('/System/Library/Frameworks/Cocoa.framework/Versions/Current/Cocoa',),
                    {'env':None, 'executable_path': None}),
            ])
            patcher.clear_calls()

            result = dyld.framework_find('Cocoa')
            self.assertEqual(patcher.calls, [
                ('dyld_find', ('Cocoa',),
                    {'env':None, 'executable_path': None}),
                ('dyld_find', ('Cocoa.framework/Cocoa',),
                    {'env':None, 'executable_path': None}),
            ])
            patcher.clear_calls()

            result = dyld.framework_find('Cocoa', '/my/sw/bin', {})
            self.assertEqual(patcher.calls, [
                ('dyld_find', ('Cocoa',),
                    {'env':{}, 'executable_path': '/my/sw/bin'}),
                ('dyld_find', ('Cocoa.framework/Cocoa',),
                    {'env':{}, 'executable_path': '/my/sw/bin'}),
            ])
            patcher.clear_calls()


        finally:
            patcher.cleanup()




class TestTrivialDyld (unittest.TestCase):
    # Tests ported from the implementation file
    def testBasic(self):
        self.assertEqual(dyld.dyld_find('libSystem.dylib'), '/usr/lib/libSystem.dylib')
        self.assertEqual(dyld.dyld_find('System.framework/System'), '/System/Library/Frameworks/System.framework/System')

if __name__ == "__main__":
    unittest.main()
