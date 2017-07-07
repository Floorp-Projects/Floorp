# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import logging

from ..util.yaml import load_yaml

logger = logging.getLogger(__name__)


def loader(kind, path, config, params, loaded_tasks):
    """
    Generate tasks implementing Gecko tests.
    """

    # the kind on which this one depends
    if len(config.get('kind-dependencies', [])) != 1:
        raise Exception(
            "Test kinds must have exactly one item in kind-dependencies")
    dep_kind = config['kind-dependencies'][0]

    # get build tasks, keyed by build platform
    builds_by_platform = get_builds_by_platform(dep_kind, loaded_tasks)

    # get the test platforms for those build tasks
    test_platforms_cfg = load_yaml(path, 'test-platforms.yml')
    test_platforms = get_test_platforms(test_platforms_cfg, builds_by_platform)

    # expand the test sets for each of those platforms
    test_sets_cfg = load_yaml(path, 'test-sets.yml')
    test_platforms = expand_tests(test_sets_cfg, test_platforms)

    # load the test descriptions
    test_descriptions = load_yaml(path, 'tests.yml', enforce_order=True)

    # generate all tests for all test platforms
    for test_platform_name, test_platform in test_platforms.iteritems():
        for test_name in test_platform['test-names']:
            test = copy.deepcopy(test_descriptions[test_name])
            test['build-platform'] = test_platform['build-platform']
            test['test-platform'] = test_platform_name
            test['build-label'] = test_platform['build-label']
            test['build-attributes'] = test_platform['build-attributes']
            test['test-name'] = test_name
            if test_platform['nightly']:
                test.setdefault('attributes', {})['nightly'] = True

            logger.debug("Generating tasks for test {} on platform {}".format(
                test_name, test['test-platform']))
            yield test


def get_builds_by_platform(dep_kind, loaded_tasks):
    """Find the build tasks on which tests will depend, keyed by
    platform/type.  Returns a dictionary mapping build platform to task."""
    builds_by_platform = {}
    for task in loaded_tasks:
        if task.kind != dep_kind:
            continue

        build_platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        if not build_platform or not build_type:
            continue
        platform = "{}/{}".format(build_platform, build_type)
        if platform in builds_by_platform:
            raise Exception("multiple build jobs for " + platform)
        builds_by_platform[platform] = task
    return builds_by_platform


def get_test_platforms(test_platforms_cfg, builds_by_platform):
    """Get the test platforms for which test tasks should be generated,
    based on the available build platforms.  Returns a dictionary mapping
    test platform to {test-set, build-platform, build-label}."""
    test_platforms = {}
    for test_platform, cfg in test_platforms_cfg.iteritems():
        build_platform = cfg['build-platform']
        if build_platform not in builds_by_platform:
            logger.warning(
                "No build task with platform {}; ignoring test platform {}".format(
                    build_platform, test_platform))
            continue
        test_platforms[test_platform] = {
            'nightly': builds_by_platform[build_platform].attributes.get('nightly', False),
            'build-platform': build_platform,
            'build-label': builds_by_platform[build_platform].label,
            'build-attributes': builds_by_platform[build_platform].attributes,
        }
        test_platforms[test_platform].update(cfg)
    return test_platforms


def expand_tests(test_sets_cfg, test_platforms):
    """Expand the test sets in `test_platforms` out to sets of test names.
    Returns a dictionary like `get_test_platforms`, with an additional
    `test-names` key for each test platform, containing a set of test
    names."""
    rv = {}
    for test_platform, cfg in test_platforms.iteritems():
        test_sets = cfg['test-sets']
        if not set(test_sets) < set(test_sets_cfg):
            raise Exception(
                "Test sets {} for test platform {} are not defined".format(
                    ', '.join(test_sets), test_platform))
        test_names = set()
        for test_set in test_sets:
            test_names.update(test_sets_cfg[test_set])
        rv[test_platform] = cfg.copy()
        rv[test_platform]['test-names'] = test_names
    return rv
