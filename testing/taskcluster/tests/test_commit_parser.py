#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
from taskcluster_graph.commit_parser import (
    parse_commit,
    normalize_test_list,
    InvalidCommitException
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
        self.assertEqual(
            normalize_test_list({}, ['woot'], 'a,b,c'),
            [{ 'test': 'a' }, { 'test': 'b' }, { 'test': 'c' }]
        )

    def test_normalize_test_list_specific_tests_with_whitespace(self):
        self.assertEqual(
            normalize_test_list({}, ['woot'], 'a, b, c'),
            [{ 'test': 'a' }, { 'test': 'b' }, { 'test': 'c' }]
        )

    def test_normalize_test_list_with_alias(self):
        self.assertEqual(
            normalize_test_list({ "a": "alpha" }, ['woot'], 'a, b, c'),
            [{ 'test': 'alpha' }, { 'test': 'b' }, { 'test': 'c' }]
        )

    def test_normalize_test_list_with_alias_and_chunk(self):
        self.assertEqual(
            normalize_test_list({ "a": "alpha" }, ['woot'], 'a-1, a-3'),
            [{ 'test': 'alpha', "only_chunks": set([1, 3])  }]
        )

    def test_invalid_commit(self):
        '''
        Disallow invalid commit messages from being parsed...
        '''
        with self.assertRaises(InvalidCommitException):
            parse_commit("wootbarbaz", {})

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
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                'additional-parameters': {}
            },
            {
                'task': 'task/linux',
                'dependents': [{
                    'allowed_build_tasks': {
                        'task/linux': {
                            'task':'task/web-platform-tests'
                        }
                    }
                }],
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

    def test_specific_test_platforms(self):
        '''
        This test cases covers the platform specific test exclusion options.
        '''
        commit = 'try: -b od -p all -u all[windows,b2g] -t none'
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
                    'platforms': ['windows'],
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
                'additional-parameters': {}
            },
            {
                'task': 'task/linux-debug',
                'dependents': [],
                'additional-parameters': {}
            },
            {
                'task': 'task/win32',
                'dependents': [
                    {
                        'allowed_build_tasks': {
                            'task/linux': {
                                'task': 'task/web-platform-tests',
                            },
                            'task/win32': {
                                'task': 'task/web-platform-tests',
                            }
                        }
                    },
                    {
                        'allowed_build_tasks': {
                            'task/linux-debug': {
                                'task': 'task/mochitest',
                            },
                            'task/win32': {
                                'task': 'task/mochitest',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
        self.assertEqual(expected, result)

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
                            },
                            'task/win32': {
                                'task': 'task/mochitest',
                            }
                        }
                    }
                ],
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                                'only_chunks': set([1, 2])
                            },
                        }
                    }
                ],
                'additional-parameters': {}
            }
        ]
        result = parse_commit(commit, jobs)
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
                ],
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
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
                ],
                'additional-parameters': {}
            }
        ]

        result = parse_commit(commit, jobs)
        self.assertEqual(expected, result)


if __name__ == '__main__':
    mozunit.main()
