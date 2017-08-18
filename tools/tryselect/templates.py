# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Templates provide a way of modifying the task definition of selected
tasks. They live under taskcluster/taskgraph/templates.
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import ArgumentParser

from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))


class TemplateParser(ArgumentParser):
    __metaclass__ = ABCMeta

    @abstractproperty
    def arguments(self):
        pass

    @abstractproperty
    def templates(self):
        pass

    def __init__(self, *args, **kwargs):
        ArgumentParser.__init__(self, *args, **kwargs)

        for cli, kwargs in self.arguments:
            self.add_argument(*cli, **kwargs)

        self.templates = {t: all_templates[t]() for t in self.templates}

        group = self.add_argument_group("template arguments")
        for template in self.templates.values():
            template.add_arguments(group)

    def parse_known_args(self, *args, **kwargs):
        args, remainder = ArgumentParser.parse_known_args(self, *args, **kwargs)

        if self.templates:
            args.templates = {}
            for name, cls in self.templates.iteritems():
                context = cls.context(**vars(args))
                if context is not None:
                    args.templates[name] = context

        return args, remainder


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
            return {'enabled': 1}

        if no_artifact:
            return

        build = MozbuildObject.from_environment(cwd=here)
        try:
            if build.substs.get("MOZ_ARTIFACT_BUILDS"):
                print("Artifact builds enabled, pass --no-artifact to disable")
                return {'enabled': 1}
        except BuildEnvironmentNotFoundException:
            pass


all_templates = {
    'artifact': Artifact,
}
