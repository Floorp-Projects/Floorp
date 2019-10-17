# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from voluptuous import (
    Optional,
    Required,
    Extra,
)

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.tests import test_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from taskgraph.util.treeherder import split_symbol, join_symbol

transforms = TransformSequence()


raptor_description_schema = Schema({
    # Raptor specific configs.
    Optional('apps'): optionally_keyed_by(
        'test-platform',
        [basestring]
    ),
    Optional('raptor-test'): basestring,
    Optional('activity'): optionally_keyed_by(
        'app',
        basestring
    ),
    Optional('binary-path'): optionally_keyed_by(
        'app',
        basestring
    ),
    Optional('cold'): optionally_keyed_by(
        'test-platform', 'app',
        bool,
    ),
    # Configs defined in the 'test_description_schema'.
    Optional('max-run-time'): optionally_keyed_by(
        'app',
        test_description_schema['max-run-time']
    ),
    Optional('run-on-projects'): optionally_keyed_by(
        'app',
        test_description_schema['run-on-projects']
    ),
    Optional('target'): optionally_keyed_by(
        'app',
        test_description_schema['target']
    ),
    Required('test-name'): test_description_schema['test-name'],
    Required('test-platform'): test_description_schema['test-platform'],
    Required('require-signed-extensions'): test_description_schema['require-signed-extensions'],
    Required('treeherder-symbol'): test_description_schema['treeherder-symbol'],
    # Any unrecognized keys will be validated against the test_description_schema.
    Extra: object,
})

transforms.add_validate(raptor_description_schema)


@transforms.add
def split_apps(config, tests):
    app_symbols = {
        'chrome': 'Chr',
        'chromium': 'Cr',
        'fenix': 'fenix',
        'refbrow': 'refbrow',
    }

    for test in tests:
        apps = test.pop('apps', None)
        if not apps:
            yield test
            continue

        for app in apps:
            atest = deepcopy(test)
            suffix = "-{}".format(app)
            atest['app'] = app
            atest['description'] += " on {}".format(app.capitalize())

            name = atest['test-name']
            if name.endswith('-cold'):
                name = atest['test-name'][:-len('-cold')] + suffix + '-cold'
            else:
                name += suffix

            atest['test-name'] = name
            atest['try-name'] = name

            if app in app_symbols:
                group, symbol = split_symbol(atest['treeherder-symbol'])
                group += "-{}".format(app_symbols[app])
                atest['treeherder-symbol'] = join_symbol(group, symbol)

            yield atest


@transforms.add
def handle_keyed_by_app(config, tests):
    fields = [
        'activity',
        'binary-path',
        'cold',
        'max-run-time',
        'run-on-projects',
        'target',
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(test, field, item_name=test['test-name'])
        yield test


@transforms.add
def split_cold(config, tests):
    for test in tests:
        cold = test.pop('cold', False)

        if not cold:
            yield test
            continue

        orig = deepcopy(test)
        yield orig

        assert 'raptor-test' in test
        test['description'] += " using cold pageload"
        test['raptor-test'] += '-cold'
        test['max-run-time'] = 3000
        test['test-name'] += '-cold'
        test['try-name'] += '-cold'

        group, symbol = split_symbol(test['treeherder-symbol'])
        symbol += '-c'
        test['treeherder-symbol'] = join_symbol(group, symbol)
        yield test


@transforms.add
def add_extra_options(config, tests):
    for test in tests:
        extra_options = test.setdefault('mozharness', {}).setdefault('extra-options', [])

        if 'app' in test:
            extra_options.append('--app={}'.format(test.pop('app')))

        if 'activity' in test:
            extra_options.append('--activity={}'.format(test.pop('activity')))

        if 'binary-path' in test:
            extra_options.append('--binary-path={}'.format(test.pop('binary-path')))

        if 'raptor-test' in test:
            extra_options.append('--test={}'.format(test.pop('raptor-test')))

        if test['require-signed-extensions']:
            extra_options.append('--is-release-build')

        # add urlparams based on platform, test names and projects
        testurlparams_by_platform_and_project = {
            "android-hw-g5": [
                {
                    "branches": [],  # For all branches
                    "testnames": ["youtube-playback"],
                    "urlparams": [
                        # param used for excluding youtube-playback tests from executing
                        # it excludes the tests with videos >1080p
                        "exclude=1,2,9,10,17,18,21,22,26,28,30,32,39,40,47,"
                        "48,55,56,63,64,71,72,79,80,83,84,89,90,95,96",
                    ]
                },
            ]
        }

        for platform, testurlparams_by_project_definitions \
                in testurlparams_by_platform_and_project.items():

            if test['test-platform'].startswith(platform):
                # For every platform it may have several definitions
                for testurlparams_by_project in testurlparams_by_project_definitions:
                    # The test should contain at least one defined testname
                    if any(
                        testname in test['test-name']
                        for testname in testurlparams_by_project['testnames']
                    ):
                        branches = testurlparams_by_project['branches']
                        if (
                            branches == [] or
                            config.params.get('project') in branches or
                            config.params.is_try() and 'try' in branches
                        ):
                            params_query = '&'.join(testurlparams_by_project['urlparams'])
                            add_extra_params_option = "--test-url-params={}".format(params_query)
                            extra_options.append(add_extra_params_option)

        yield test
