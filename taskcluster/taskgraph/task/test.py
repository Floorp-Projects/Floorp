# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import logging
import os
import yaml

from . import base
from taskgraph.util.python_path import find_object
from taskgraph.transforms.base import TransformSequence, TransformConfig

logger = logging.getLogger(__name__)


class TestTask(base.Task):
    """
    A task implementing a Gecko test.
    """

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):

        # the kind on which this one depends
        if len(config.get('kind-dependencies', [])) != 1:
            raise Exception("TestTask kinds must have exactly one item in kind-dependencies")
        dep_kind = config['kind-dependencies'][0]

        # get build tasks, keyed by build platform
        builds_by_platform = cls.get_builds_by_platform(dep_kind, loaded_tasks)

        # get the test platforms for those build tasks
        test_platforms_cfg = load_yaml(path, 'test-platforms.yml')
        test_platforms = cls.get_test_platforms(test_platforms_cfg, builds_by_platform)

        # expand the test sets for each of those platforms
        test_sets_cfg = load_yaml(path, 'test-sets.yml')
        test_platforms = cls.expand_tests(test_sets_cfg, test_platforms)

        # load the test descriptions
        test_descriptions = load_yaml(path, 'tests.yml')

        # load the transformation functions
        transforms = cls.load_transforms(config['transforms'])

        # generate all tests for all test platforms
        def tests():
            for test_platform_name, test_platform in test_platforms.iteritems():
                for test_name in test_platform['test-names']:
                    test = copy.deepcopy(test_descriptions[test_name])
                    test['build-platform'] = test_platform['build-platform']
                    test['test-platform'] = test_platform_name
                    test['build-label'] = test_platform['build-label']
                    test['test-name'] = test_name
                    yield test

        # log each source test definition as it is created; this helps when debugging
        # an exception in task generation
        def log_tests(config, tests):
            for test in tests:
                logger.debug("Generating tasks for {} test {} on platform {}".format(
                    kind, test['test-name'], test['test-platform']))
                yield test
        transforms = TransformSequence([log_tests] + transforms)

        # perform the transformations
        config = TransformConfig(kind, path, config, params)
        tasks = [cls(config.kind, t) for t in transforms(config, tests())]
        return tasks

    @classmethod
    def get_builds_by_platform(cls, dep_kind, loaded_tasks):
        """Find the build tasks on which tests will depend, keyed by
        platform/type.  Returns a dictionary mapping build platform to task
        label."""
        builds_by_platform = {}
        for task in loaded_tasks:
            if task.kind != dep_kind:
                continue
            # remove this check when builds are no longer legacy
            if task.attributes['legacy_kind'] != 'build':
                continue

            build_platform = task.attributes.get('build_platform')
            build_type = task.attributes.get('build_type')
            if not build_platform or not build_type:
                continue
            platform = "{}/{}".format(build_platform, build_type)
            if platform in builds_by_platform:
                raise Exception("multiple build jobs for " + platform)
            builds_by_platform[platform] = task.label
        return builds_by_platform

    @classmethod
    def get_test_platforms(cls, test_platforms_cfg, builds_by_platform):
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
                'test-set': cfg['test-set'],
                'build-platform': build_platform,
                'build-label': builds_by_platform[build_platform],
            }
        return test_platforms

    @classmethod
    def expand_tests(cls, test_sets_cfg, test_platforms):
        """Expand the test sets in `test_platforms` out to sets of test names.
        Returns a dictionary like `get_test_platforms`, with an additional
        `test-names` key for each test platform, containing a set of test
        names."""
        rv = {}
        for test_platform, cfg in test_platforms.iteritems():
            test_set = cfg['test-set']
            if test_set not in test_sets_cfg:
                raise Exception(
                    "Test set '{}' for test platform {} is not defined".format(
                        test_set, test_platform))
            test_names = test_sets_cfg[test_set]
            rv[test_platform] = cfg.copy()
            rv[test_platform]['test-names'] = test_names
        return rv

    @classmethod
    def load_transforms(cls, transforms_cfg):
        """Load the transforms specified in kind.yml"""
        transforms = []
        for path in transforms_cfg:
            transform = find_object(path)
            transforms.append(transform)
        return transforms

    @classmethod
    def from_json(cls, task_dict):
        test_task = cls(kind=task_dict['attributes']['kind'],
                        task=task_dict)
        return test_task

    def __init__(self, kind, task):
        self.dependencies = task['dependencies']
        super(TestTask, self).__init__(kind, task['label'], task['attributes'], task['task'])

    def get_dependencies(self, taskgraph):
        return [(label, name) for name, label in self.dependencies.items()]

    def optimize(self):
        return False, None


def load_yaml(path, name):
    """Convenience method to load a YAML file in the kind directory"""
    filename = os.path.join(path, name)
    with open(filename, "rb") as f:
        return yaml.load(f)
