# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


import argparse
import copy
import functools
import re
import shlex
from try_test_parser import parse_test_opts

TRY_DELIMITER = 'try:'
TEST_CHUNK_SUFFIX = re.compile('(.*)-([0-9]+)$')

# The build type aliases are very cryptic and only used in try flags these are
# mappings from the single char alias to a longer more recognizable form.
BUILD_TYPE_ALIASES = {
    'o': 'opt',
    'd': 'debug'
}

class InvalidCommitException(Exception):
    pass

def escape_whitspace_in_brackets(input_str):
    '''
    In tests you may restrict them by platform [] inside of the brackets
    whitespace may occur this is typically invalid shell syntax so we escape it
    with backslash sequences    .
    '''
    result = ""
    in_brackets = False
    for char in input_str:
        if char == '[':
            in_brackets = True
            result += char
            continue

        if char == ']':
            in_brackets = False
            result += char
            continue

        if char == ' ' and in_brackets:
            result += '\ '
            continue

        result += char

    return result


def normalize_platform_list(alias, all_builds, build_list):
    if build_list == 'all':
        return all_builds

    results = []
    for build in build_list.split(','):
        if build in alias:
            build = alias[build]
        results.append(build)

    return results

def normalize_test_list(aliases, all_tests, job_list):
    '''
    Normalize a set of jobs (builds or tests) there are three common cases:

        - job_list is == 'none' (meaning an empty list)
        - job_list is == 'all' (meaning use the list of jobs for that job type)
        - job_list is comma delimited string which needs to be split

    :param dict aliases: Alias mapping for jobs...
    :param list all_tests: test flags from job_flags.yml structure.
    :param str job_list: see above examples.
    :returns: List of jobs
    '''

    # Empty job list case...
    if job_list is None or job_list == 'none':
        return []

    tests = parse_test_opts(job_list)

    if not tests:
        return []

    # Special case where tests is 'all' and must be expanded
    if tests[0]['test'] == 'all':
        results = []
        all_entry = tests[0]
        for test in all_tests:
            entry = { 'test': test }
            # If there are platform restrictions copy them across the list.
            if 'platforms' in all_entry:
                entry['platforms'] = list(all_entry['platforms'])
            results.append(entry)
        return parse_test_chunks(aliases, results)
    else:
        return parse_test_chunks(aliases, tests)

def parse_test_chunks(aliases, tests):
    '''
    Test flags may include parameters to narrow down the number of chunks in a
    given push. We don't model 1 chunk = 1 job in taskcluster so we must check
    each test flag to see if it is actually specifying a chunk.

    :param dict aliases: Dict of alias name -> real name.
    :param list tests: Result from normalize_test_list
    :returns: List of jobs
    '''
    results = []
    seen_chunks = {}
    for test in tests:
        matches = TEST_CHUNK_SUFFIX.match(test['test'])

        if not matches:
            if test['test'] in aliases:
                test['test'] = aliases[test['test']]
            results.append(test)
            continue

        name = matches.group(1)
        chunk = int(matches.group(2))

        if name in aliases:
            name = aliases[name]

        if name in seen_chunks:
            seen_chunks[name].add(chunk)
        else:
            seen_chunks[name] = set([chunk])
            test['test'] = name
            test['only_chunks'] = seen_chunks[name]
            results.append(test)

    return results;

def extract_tests_from_platform(test_jobs, build_platform, build_task, tests):
    '''
    Build the list of tests from the current build.

    :param dict test_jobs: Entire list of tests (from job_flags.yml).
    :param dict build_platform: Current build platform.
    :param str build_task: Build task path.
    :param list tests: Test flags.
    :return: List of tasks (ex: [{ task: 'test_task.yml' }]
    '''
    if tests is None:
        return []

    results = []

    for test_entry in tests:
        if test_entry['test'] not in test_jobs:
            continue

        test_job = test_jobs[test_entry['test']]

        # Verify that this job can actually be run on this build task...
        if 'allowed_build_tasks' in test_job and build_task not in test_job['allowed_build_tasks']:
            continue

        if 'platforms' in test_entry:
            # The default here is _exclusive_ rather then inclusive so if the
            # build platform does not specify what platform(s) it belongs to
            # then we must skip it.
            if 'platforms' not in build_platform:
                continue

            # Sorta hack to see if the two lists intersect at all if they do not
            # then we must skip this set.
            common_platforms = set(test_entry['platforms']) & set(build_platform['platforms'])
            if not common_platforms:
                # Tests should not run on this platform...
                continue

        # Add the job to the list and ensure to copy it so we don't accidentally
        # mutate the state of the test job in the future...
        specific_test_job = copy.deepcopy(test_job)

        # Update the task configuration for all tests in the matrix...
        for build_name in specific_test_job:
            for test_task_name in specific_test_job[build_name]:
                test_task = specific_test_job[build_name][test_task_name]
                # Copy over the chunk restrictions if given...
                if 'only_chunks' in test_entry:
                    test_task['only_chunks'] = \
                            copy.copy(test_entry['only_chunks'])

        results.append(specific_test_job)

    return results

'''
This module exists to deal with parsing the options flags that try uses. We do
not try to build a graph or anything here but match up build flags to tasks via
the "jobs" datastructure (see job_flags.yml)
'''

def parse_commit(message, jobs):
    '''
    :param message: Commit message that is typical to a try push.
    :param jobs: Dict (see job_flags.yml)
    '''

    # shlex used to ensure we split correctly when giving values to argparse.
    parts = shlex.split(escape_whitspace_in_brackets(message))
    try_idx = None
    for idx, part in enumerate(parts):
        if part == TRY_DELIMITER:
            try_idx = idx
            break

    if try_idx is None:
        raise InvalidCommitException('Invalid commit format contain ' +
                TRY_DELIMITER)

    # Argument parser based on try flag flags
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--build', dest='build_types')
    parser.add_argument('-p', '--platform', nargs='?', dest='platforms', const='all', default='all')
    parser.add_argument('-u', '--unittests', nargs='?', dest='tests', const='all', default='all')
    parser.add_argument('-i', '--interactive', dest='interactive', action='store_true', default=False)
    args, unknown = parser.parse_known_args(parts[try_idx:])

    # Then builds...
    if args.build_types is None:
        return []

    build_types = [ BUILD_TYPE_ALIASES.get(build_type, build_type) for
            build_type in args.build_types ]

    aliases = jobs['flags'].get('aliases', {})

    platforms = normalize_platform_list(aliases, jobs['flags']['builds'], args.platforms)
    tests = normalize_test_list(aliases, jobs['flags']['tests'], args.tests)

    result = []

    # Expand the matrix of things!
    for platform in platforms:
        # Silently skip unknown platforms.
        if platform not in jobs['builds']:
            continue

        platform_builds = jobs['builds'][platform]

        for build_type in build_types:
            # Not all platforms have debug builds, etc...
            if build_type not in platform_builds['types']:
                continue

            platform_build = platform_builds['types'][build_type]
            build_task = platform_build['task']

            if 'additional-parameters' in platform_build:
                additional_parameters = platform_build['additional-parameters']
            else:
                additional_parameters = {}

            # Generate list of post build tasks that run on this build
            post_build_jobs = []
            for job_flag in jobs['flags'].get('post-build', []):
                job = jobs['post-build'][job_flag]
                if ('allowed_build_tasks' in job and
                        build_task not in job['allowed_build_tasks']):
                    continue
                post_build_jobs.append(copy.deepcopy(job))

            # Node for this particular build type
            result.append({
                'task': build_task,
                'post-build': post_build_jobs,
                'dependents': extract_tests_from_platform(
                    jobs['tests'], platform_builds, build_task, tests
                ),
                'additional-parameters': additional_parameters,
                'build_name': platform,
                'build_type': build_type,
                'interactive': args.interactive,
            })

    return result
