# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Templates provide a way of modifying the task definition of selected
tasks. They live under taskcluster/taskgraph/templates.
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
from abc import ABCMeta, abstractmethod

from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))


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
            return {'enabled': '1'}

        if no_artifact:
            return

        build = MozbuildObject.from_environment(cwd=here)
        try:
            if build.substs.get("MOZ_ARTIFACT_BUILDS"):
                print("Artifact builds enabled, pass --no-artifact to disable")
                return {'enabled': '1'}
        except BuildEnvironmentNotFoundException:
            pass


class Environment(Template):

    def add_arguments(self, parser):
        parser.add_argument('--env', action='append', default=None,
                            help='Set an environment variable, of the form FOO=BAR. '
                                 'Can be passed in multiple times.')

    def context(self, env, **kwargs):
        if not env:
            return
        return dict(e.split('=', 1) for e in env)


all_templates = {
    'artifact': Artifact,
    'env': Environment,
}
