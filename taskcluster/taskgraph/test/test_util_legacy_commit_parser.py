#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
from taskgraph.util.legacy_commit_parser import (
    parse_commit,
    normalize_test_list,
    parse_test_opts
)

class TestCommitParser(unittest.TestCase):

    def test_normalize_test_list_none(self):
        self.assertEqual(
            normalize_test_list({}, ['woot'], 'none'), []
        )

    def test_normalize_test_list_all(self):
        self.assertEqual(
            normalize_test_list({}, ['woot'], 'all'),
            [{ 'test': 'woot' }]
        )

    def test_normalize_test_list_specific_tests(self):
        self.assertEqual(sorted(
            normalize_test_list({}, ['woot'], 'a,b,c')),
            sorted([{ 'test': 'a' }, { 'test': 'b' }, { 'test': 'c' }])
        )

    def test_normalize_test_list_specific_tests_with_whitespace(self):
        self.assertEqual(sorted(
            normalize_test_list({}, ['woot'], 'a, b, c')),
            sorted([{ 'test': 'a' }, { 'test': 'b' }, { 'test': 'c' }])
        )

    def test_normalize_test_list_with_alias(self):
        self.assertEqual(sorted(
            normalize_test_list({ "a": "alpha" }, ['woot'], 'a, b, c')),
            sorted([{ 'test': 'alpha' }, { 'test': 'b' }, { 'test': 'c' }])
        )

    def test_normalize_test_list_with_alias_and_chunk(self):
        self.assertEqual(
            normalize_test_list({ "a": "alpha" }, ['woot'], 'a-1, a-3'),
            [{ 'test': 'alpha', "only_chunks": set([1, 3])  }]
        )

    def test_normalize_test_list_with_alias_pattern(self):
        self.assertEqual(sorted(
            normalize_test_list({ "a": '/.*oo.*/' },
                                ['woot', 'foo', 'bar'],
                                'a, b, c')),
            sorted([{ 'test': t } for t in ['woot', 'foo', 'b', 'c']])
        )

    def test_normalize_test_list_with_alias_pattern_anchored(self):
        self.assertEqual(sorted(
            normalize_test_list({ "a": '/.*oo/' },
                                ['woot', 'foo', 'bar'],
                                'a, b, c')),
            sorted([{ 'test': t } for t in ['foo', 'b', 'c']])
        )

    def test_normalize_test_list_with_alias_pattern_list(self):
        self.assertEqual(sorted(
            normalize_test_list({ "a": ['/.*oo/', 'bar', '/bi.*/'] },
                                ['woot', 'foo', 'bar', 'bing', 'baz'],
                                'a, b')),
            sorted([{ 'test': t } for t in ['foo', 'bar', 'bing', 'b']])
        )

    def test_normalize_test_list_with_alias_pattern_list_chunks(self):
        self.assertEqual(sorted(
            normalize_test_list({ "a": ['/.*oo/', 'bar', '/bi.*/'] },
                                ['woot', 'foo', 'bar', 'bing', 'baz'],
                                'a-1, a-4, b')),
            sorted([{'test': 'b'}] + [
                { 'test': t, 'only_chunks': set([1, 4])} for t in ['foo', 'bar', 'bing']])
        )

    def test_commit_no_tests(self):
        '''
        This test covers the case of builds but no tests passed -u none
        '''
        commit = 'try: -b o -p linux -u none -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {}
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [],
                'additional-parameters': {},
                'build_name': 'linux',
                'build_type': 'opt',
                'post-build': [],
                'interactive': False,
                'when': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_flag_aliasing(self):
        commit = 'try: -b o -p magic-alias -u none -t none'
        jobs = {
            'flags': {
                'aliases': {
                    'magic-alias': 'linux'
                },
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {}
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [],
                'additional-parameters': {},
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'post-build': [],
                'when': {},
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_try_flag_in_middle_of_commit(self):
        '''
        The try command prefix may appear anywhere this test ensures that it
        works in some common cases.
        '''
        commit = 'Bug XXX - I like cheese try: -b o -p all -u none wootbar'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {}
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [],
                'additional-parameters': {},
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'post-build': [],
                'when': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_try_flags_not_specified(self):
        '''
        Try flags are optional, and if not provided, should cause an empty graph
        to be generated.
        '''
        commit = 'Bug XXX - test commit with no flags'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {}
        }

        expected = []

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_commit_all_builds_no_tests(self):
        '''
        This test covers the case of all builds but no tests passed -u none
        '''
        commit = 'try: -b o -p all -u none -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {}
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [],
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'additional-parameters': {},
                'when': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_some_test_tasks_restricted(self):
        '''
        This test covers the case of all builds but no tests passed -u none
        '''
        commit = 'try: -b do -p all -u all -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {
                'web-platform-tests': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/web-platform-tests',
                        }
                    }
                }
            }
        }

        expected = [
            {
                'task': 'task/linux-debug',
                'dependents': [],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'debug',
                'interactive': False,
                'when': {},
            },
            {
                'task': 'task/linux',
                'dependents': [{
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task':'task/web-platform-tests',
                            'unittest_try_name':'web-platform-tests'
                        }
                    }
                }],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'when': {},
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)


    def test_specific_test_platforms(self):
        '''
        This test cases covers the platform specific test exclusion options.
        Intentionally includes platforms with spaces.
        '''
        commit = 'try: -b od -p all -u all[Windows XP,b2g] -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'win32'],
                'tests': ['web-platform-tests', 'mochitest'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
                'win32': {
                    'platforms': ['Windows XP'],
                    'types': {
                        'opt': {
                            'task': 'task/win32',
                        }
                    }
                },
            },
            'tests': {
                'web-platform-tests': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/win32': {
                            'task': 'task/web-platform-tests',
                        }
                    }
                },
                'mochitest': {
                    'allowed_build_tasks': {
                        'task/linux-debug': {
                            'task': 'task/mochitest',
                        },
                        'task/win32': {
                            'task': 'task/mochitest',
                        }
                    }
                }
            }
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'when': {},
            },
            {
                'task': 'task/linux-debug',
                'dependents': [],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'debug',
                'interactive': False,
                'when': {},
            },
            {
                'task': 'task/win32',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/win32': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    },
                    {
                        'allowed_build_tasks': {
                            'task/linux-debug': {
                                'task': 'task/mochitest',
                                'unittest_try_name': 'mochitest',
                            },
                            'task/win32': {
                                'task': 'task/mochitest',
                                'unittest_try_name': 'mochitest',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'win32',
                'build_type': 'opt',
                'interactive': False,
                'when': {},
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(sorted(expected), sorted(result))

    def test_specific_test_platforms_with_specific_platform(self):
        '''
        This test cases covers the platform specific test exclusion options.
        '''
        commit = 'try: -b od -p win32 -u mochitest[windows] -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'win32'],
                'tests': ['web-platform-tests', 'mochitest'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux'
                        },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
                'win32': {
                    'platforms': ['windows'],
                    'types': {
                        'opt': {
                            'task': 'task/win32'
                        }
                    }
                },
            },
            'tests': {
                'web-platform-tests': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/win32': {
                            'task': 'task/web-platform-tests',
                        }
                    }
                },
                'mochitest': {
                    'allowed_build_tasks': {
                        'task/linux-debug': {
                            'task': 'task/mochitest',
                        },
                        'task/win32': {
                            'task': 'task/mochitest',
                        }
                    }
                }
            }
        }

        expected = [
            {
                'task': 'task/win32',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux-debug': {
                                'task': 'task/mochitest',
                                'unittest_try_name': 'mochitest',
                            },
                            'task/win32': {
                                'task': 'task/mochitest',
                                'unittest_try_name': 'mochitest',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'win32',
                'build_type': 'opt',
                'interactive': False,
                'when': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_specific_chunks(self):
        '''
        This test covers specifying specific chunks for a given test suite.
        '''
        commit = 'try: -b o -p linux -u mochitest-1,mochitest-2 -t none'
        jobs = {
            'flags': {
                'builds': ['linux'],
                'tests': ['mochitest'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
            },
            'tests': {
                'mochitest': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/mochitest',
                            'chunks': 5
                        },
                    }
                }
            }
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/mochitest',
                                'chunks': 5,
                                'only_chunks': set([1, 2]),
                                'unittest_try_name': 'mochitest',
                            },
                        }
                    }
                ],
                'additional-parameters': {},
                'post-build': [],
                'build_name': 'linux',
                'build_type': 'opt',
                'interactive': False,
                'when': {},
            }
        ]
        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_commit_with_builds_and_tests(self):
        '''
        This test covers the broad case of a commit which has both builds and
        tests without any exclusions or other fancy logic.
        '''
        commit = 'try: -b od -p linux,linux64 -u web-platform-tests -t none'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
                'linux64': {
                    'types': {
                        'opt': {
                            'task': 'task/linux64',
                         },
                        'debug': {
                            'task': 'task/linux64-debug'
                        }
                    }
                }
            },
            'tests': {
                'web-platform-tests': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux-debug': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux64': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux64-debug': {
                            'task': 'task/web-platform-tests',
                        }
                    }
                }
            }
        }

        expected = [
            {
                'task': 'task/linux',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            },
            {
                'task': 'task/linux-debug',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            },
            {
                'task': 'task/linux64',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            },
            {
                'task': 'task/linux64-debug',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_commit_with_builds_and_tests(self):
        '''
        This tests the long form of the try flags.
        '''
        commit = 'try: --build od --platform linux,linux64 --unittests web-platform-tests --talos none'
        jobs = {
            'flags': {
                'builds': ['linux', 'linux64'],
                'tests': ['web-platform-tests'],
            },
            'builds': {
                'linux': {
                    'types': {
                        'opt': {
                            'task': 'task/linux',
                         },
                        'debug': {
                            'task': 'task/linux-debug'
                        }
                    }
                },
                'linux64': {
                    'types': {
                        'opt': {
                            'task': 'task/linux64',
                         },
                        'debug': {
                            'task': 'task/linux64-debug'
                        }
                    }
                }
            },
            'tests': {
                'web-platform-tests': {
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux-debug': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux64': {
                            'task': 'task/web-platform-tests',
                        },
                        'task/linux64-debug': {
                            'task': 'task/web-platform-tests',
                        }
                    }
                }
            }
        }
        expected = [
            {
                'task': 'task/linux',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'build_name': 'linux',
                'build_type': 'opt',
                'post-build': [],
                'interactive': False,
                'when': {}
            },
            {
                'task': 'task/linux-debug',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'build_name': 'linux',
                'build_type': 'debug',
                'post-build': [],
                'interactive': False,
                'when': {}
            },
            {
                'task': 'task/linux64',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'build_name': 'linux64',
                'build_type': 'opt',
                'post-build': [],
                'interactive': False,
                'when': {}
            },
            {
                'task': 'task/linux64-debug',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            },
                            'task/linux64-debug': {
                                'task': 'task/web-platform-tests',
                                'unittest_try_name': 'web-platform-tests',
                            }
                        }
                    }
                ],
                'additional-parameters': {},
                'build_name': 'linux64',
                'build_type': 'debug',
                'post-build': [],
                'interactive': False,
                'when': {}
            }
        ]

        result, triggers = parse_commit(commit, jobs)
        self.assertEqual(sorted(expected), sorted(result))


class TryTestParserTest(unittest.TestCase):

    def test_parse_opts_valid(self):
        self.assertEquals(
            parse_test_opts('all[Mulet Linux]'),
            [{ 'test': 'all', 'platforms': ['Mulet Linux'] }]
        )

        self.assertEquals(
            parse_test_opts('all[Amazing, Foobar woot,yeah]'),
            [{ 'test': 'all', 'platforms': ['Amazing', 'Foobar woot', 'yeah'] }]
        )

        self.assertEquals(
            parse_test_opts('a,b, c'),
            [
                { 'test': 'a' },
                { 'test': 'b' },
                { 'test': 'c' },
            ]
        )
        self.assertEquals(
            parse_test_opts('woot, bar[b], baz, qux[ z ],a'),
            [
                { 'test': 'woot' },
                { 'test': 'bar', 'platforms': ['b'] },
                { 'test': 'baz' },
                { 'test': 'qux', 'platforms': ['z'] },
                { 'test': 'a' }
            ]
        )

        self.assertEquals(
            parse_test_opts('mochitest-3[Ubuntu,10.6,10.8,Windows XP,Windows 7,Windows 8]'),
            [
                {
                    'test': 'mochitest-3',
                    'platforms': [
                        'Ubuntu', '10.6', '10.8', 'Windows XP', 'Windows 7', 'Windows 8'
                    ]
                }
            ]
        )

        self.assertEquals(
            parse_test_opts(''),
            []
        )


if __name__ == '__main__':
    mozunit.main()
