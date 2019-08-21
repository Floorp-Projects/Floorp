# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Templates provide a way of modifying the task definition of selected
tasks. They live under taskcluster/taskgraph/templates.
"""

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import sys
from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import Action, SUPPRESS

import mozpack.path as mozpath
from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject
from .tasks import resolve_tests_by_suite

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


class TryConfig(object):
    __metaclass__ = ABCMeta

    def __init__(self):
        self.dests = set()

    def add_arguments(self, parser):
        for cli, kwargs in self.arguments:
            action = parser.add_argument(*cli, **kwargs)
            self.dests.add(action.dest)

    @abstractproperty
    def arguments(self):
        pass

    @abstractmethod
    def try_config(self, **kwargs):
        pass


class Template(TryConfig):
    def try_config(self, **kwargs):
        context = self.context(**kwargs)
        if context:
            return {'templates': context}

    @abstractmethod
    def context(self, **kwargs):
        pass


class Artifact(Template):

    arguments = [
        [['--artifact'],
         {'action': 'store_true',
          'help': 'Force artifact builds where possible.'
          }],
        [['--no-artifact'],
         {'action': 'store_true',
          'help': 'Disable artifact builds even if being used locally.',
          }],
    ]

    def add_arguments(self, parser):
        group = parser.add_mutually_exclusive_group()
        return super(Artifact, self).add_arguments(group)

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

    arguments = [
        [['paths'],
         {'nargs': '*',
          'default': [],
          'help': 'Run tasks containing tests under the specified path(s).',
          }],
    ]

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
                'MOZHARNESS_TEST_PATHS': json.dumps(resolve_tests_by_suite(paths)),
            }
        }


class Environment(Template):

    arguments = [
        [['--env'],
         {'action': 'append',
          'default': None,
          'help': 'Set an environment variable, of the form FOO=BAR. '
                  'Can be passed in multiple times.',
          }],
    ]

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

    arguments = [
        [['--rebuild'],
         {'action': RangeAction,
          'min': 2,
          'max': 20,
          'default': None,
          'type': int,
          'help': 'Rebuild all selected tasks the specified number of times.',
          }],
    ]

    def context(self, rebuild, **kwargs):
        if not rebuild:
            return

        return {
            'rebuild': rebuild,
        }


class ChemspillPrio(Template):

    arguments = [
        [['--chemspill-prio'],
         {'action': 'store_true',
          'help': 'Run at a higher priority than most try jobs (chemspills only).',
          }],
    ]

    def context(self, chemspill_prio, **kwargs):
        if chemspill_prio:
            return {
                'chemspill-prio': {}
            }


class GeckoProfile(Template):
    arguments = [
        [['--gecko-profile'],
         {'dest': 'profile',
          'action': 'store_true',
          'default': False,
          'help': 'Create and upload a gecko profile during talos/raptor tasks.',
          }],
        # For backwards compatibility
        [['--talos-profile'],
         {'dest': 'profile',
          'action': 'store_true',
          'default': False,
          'help': SUPPRESS,
          }],
        # This is added for consistency with the 'syntax' selector
        [['--geckoProfile'],
         {'dest': 'profile',
          'action': 'store_true',
          'default': False,
          'help': SUPPRESS,
          }],
    ]

    def context(self, profile, **kwargs):
        if not profile:
            return
        return {'gecko-profile': profile}


class Browsertime(TryConfig):
    arguments = [
        [['--browsertime'],
         {'action': 'store_true',
          'help': 'Use browsertime during Raptor tasks.',
          }],
    ]

    def try_config(self, browsertime, **kwargs):
        if browsertime:
            return {
                'browsertime': True,
            }


class DisablePgo(TryConfig):

    arguments = [
        [['--disable-pgo'],
         {'action': 'store_true',
          'help': 'Don\'t run PGO builds',
          }],
    ]

    def try_config(self, disable_pgo, **kwargs):
        if disable_pgo:
            return {
                'disable-pgo': True,
            }


all_templates = {
    'artifact': Artifact,
    'browsertime': Browsertime,
    'chemspill-prio': ChemspillPrio,
    'disable-pgo': DisablePgo,
    'env': Environment,
    'gecko-profile': GeckoProfile,
    'path': Path,
    'rebuild': Rebuild,
}
