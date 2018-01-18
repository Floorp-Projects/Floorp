# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Templates provide a way of modifying the task definition of selected
tasks. They live under taskcluster/taskgraph/templates.
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
from abc import ABCMeta, abstractmethod
from argparse import Action

import mozpack.path as mozpath
from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


class Template(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def add_arguments(self, parser):
        pass

    @abstractmethod
    def context(self, **kwargs):
        pass


class Artifact(Template):

    def add_arguments(self, parser):
        group = parser.add_mutually_exclusive_group()
        group.add_argument('--artifact', action='store_true',
                           help='Force artifact builds where possible.')
        group.add_argument('--no-artifact', action='store_true',
                           help='Disable artifact builds even if being used locally.')

    def context(self, artifact, no_artifact, **kwargs):
        if artifact:
            return {
                'artifact': {'enabled': '1'}
            }

        if no_artifact:
            return

        try:
            if build.substs.get("MOZ_ARTIFACT_BUILDS"):
                print("Artifact builds enabled, pass --no-artifact to disable")
                return {
                    'artifact': {'enabled': '1'}
                }
        except BuildEnvironmentNotFoundException:
            pass


class Path(Template):

    def add_arguments(self, parser):
        parser.add_argument('paths', nargs='*',
                            help='Run tasks containing tests under the specified path(s).')

    def context(self, paths, **kwargs):
        if not paths:
            return

        for p in paths:
            if not os.path.exists(p):
                print("error: '{}' is not a valid path.".format(p), file=sys.stderr)
                sys.exit(1)

        paths = [mozpath.relpath(mozpath.join(os.getcwd(), p), build.topsrcdir) for p in paths]
        return {
            'env': {
                # can't use os.pathsep as machine splitting could be a different platform
                'MOZHARNESS_TEST_PATHS': ':'.join(paths),
            }
        }


class Environment(Template):

    def add_arguments(self, parser):
        parser.add_argument('--env', action='append', default=None,
                            help='Set an environment variable, of the form FOO=BAR. '
                                 'Can be passed in multiple times.')

    def context(self, env, **kwargs):
        if not env:
            return
        return {
            'env': dict(e.split('=', 1) for e in env),
        }


class RangeAction(Action):
    def __init__(self, min, max, *args, **kwargs):
        self.min = min
        self.max = max
        kwargs['metavar'] = '[{}-{}]'.format(self.min, self.max)
        super(RangeAction, self).__init__(*args, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        name = option_string or self.dest
        if values < self.min:
            parser.error('{} can not be less than {}'.format(name, self.min))
        if values > self.max:
            parser.error('{} can not be more than {}'.format(name, self.max))
        setattr(namespace, self.dest, values)


class Rebuild(Template):

    def add_arguments(self, parser):
        parser.add_argument('--rebuild', action=RangeAction, min=2, max=20, default=None, type=int,
                            help='Rebuild all selected tasks the specified number of times.')

    def context(self, rebuild, **kwargs):
        if not rebuild:
            return

        return {
            'rebuild': rebuild,
        }


all_templates = {
    'artifact': Artifact,
    'path': Path,
    'env': Environment,
    'rebuild': Rebuild,
}
