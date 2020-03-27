# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Templates provide a way of modifying the task definition of selected tasks.
They are added to 'try_task_config.json' and processed by the transforms.
"""

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import six
import subprocess
import sys
from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import Action, SUPPRESS
from textwrap import dedent

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


class Artifact(TryConfig):

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

    @classmethod
    def is_artifact_build(cls):
        try:
            return build.substs.get("MOZ_ARTIFACT_BUILDS", False)
        except BuildEnvironmentNotFoundException:
            return False

    def try_config(self, artifact, no_artifact, **kwargs):
        if artifact:
            return {
                'use-artifact-builds': True
            }

        if no_artifact:
            return

        if self.is_artifact_build():
            print("Artifact builds enabled, pass --no-artifact to disable")
            return {
                'use-artifact-builds': True
            }


class Pernosco(TryConfig):
    arguments = [
        [['--pernosco'],
         {'action': 'store_true',
          'default': None,
          'help': 'Opt-in to analysis by the Pernosco debugging service.',
          }],
        [['--no-pernosco'],
         {'dest': 'pernosco',
          'action': 'store_false',
          'default': None,
          'help': 'Opt-out of the Pernosco debugging service (if you are on the whitelist).',
          }],
    ]

    def add_arguments(self, parser):
        group = parser.add_mutually_exclusive_group()
        return super(Pernosco, self).add_arguments(group)

    def try_config(self, pernosco, **kwargs):
        if pernosco is None:
            return

        if pernosco:
            if not kwargs['no_artifact'] and (kwargs['artifact'] or Artifact.is_artifact_build()):
                print("Pernosco does not support artifact builds at this time. "
                      "Please try again with '--no-artifact'.")
                sys.exit(1)

            try:
                # The Pernosco service currently requires a Mozilla e-mail address to
                # log in. Prevent people with non-Mozilla addresses from using this
                # flag so they don't end up consuming time and resources only to
                # realize they can't actually log in and see the reports.
                output = subprocess.check_output(['ssh', '-G', 'hg.mozilla.org']).splitlines()
                address = [l.rsplit(' ', 1)[-1] for l in output if l.startswith('user')][0]
                if not address.endswith('@mozilla.com'):
                    print(dedent("""\
                        Pernosco requires a Mozilla e-mail address to view its reports. Please
                        push to try with an @mozilla.com address to use --pernosco.

                            Current user: {}
                    """.format(address)))
                    sys.exit(1)

            except (subprocess.CalledProcessError, IndexError):
                print("warning: failed to detect current user for 'hg.mozilla.org'")
                print("Pernosco requires a Mozilla e-mail address to view its reports.")
                while True:
                    answer = raw_input("Do you have an @mozilla.com address? [Y/n]: ").lower()
                    if answer == 'n':
                        sys.exit(1)
                    elif answer == 'y':
                        break

        return {
            'env': {
                'PERNOSCO': str(int(pernosco)),
            }
        }


class Path(TryConfig):

    arguments = [
        [['paths'],
         {'nargs': '*',
          'default': [],
          'help': 'Run tasks containing tests under the specified path(s).',
          }],
    ]

    def try_config(self, paths, **kwargs):
        if not paths:
            return

        for p in paths:
            if not os.path.exists(p):
                print("error: '{}' is not a valid path.".format(p), file=sys.stderr)
                sys.exit(1)

        paths = [mozpath.relpath(mozpath.join(os.getcwd(), p), build.topsrcdir) for p in paths]
        return {
            'env': {
                'MOZHARNESS_TEST_PATHS': six.ensure_text(
                    json.dumps(resolve_tests_by_suite(paths))),
            }
        }


class Environment(TryConfig):

    arguments = [
        [['--env'],
         {'action': 'append',
          'default': None,
          'help': 'Set an environment variable, of the form FOO=BAR. '
                  'Can be passed in multiple times.',
          }],
    ]

    def try_config(self, env, **kwargs):
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


class Rebuild(TryConfig):

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

    def try_config(self, rebuild, **kwargs):
        if not rebuild:
            return

        return {
            'rebuild': rebuild,
        }


class Routes(TryConfig):
    arguments = [
        [
            ["--route"],
            {
                "action": "append",
                "dest": "routes",
                "help": (
                    "Additional route to add to the tasks "
                    "(note: these will not be added to the decision task)"
                ),
            },
        ],
    ]

    def try_config(self, routes, **kwargs):
        if routes:
            return {
                'routes': routes,
            }


class ChemspillPrio(TryConfig):

    arguments = [
        [['--chemspill-prio'],
         {'action': 'store_true',
          'help': 'Run at a higher priority than most try jobs (chemspills only).',
          }],
    ]

    def try_config(self, chemspill_prio, **kwargs):
        if chemspill_prio:
            return {
                'chemspill-prio': {}
            }


class GeckoProfile(TryConfig):
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

    def try_config(self, profile, **kwargs):
        if profile:
            return {
                'gecko-profile': True,
            }


class OptimizeStrategies(TryConfig):

    arguments = [
        [['--strategy'],
         {'default': None,
          'help': 'Override the default optimization strategy. Valid values '
                  'are the experimental strategies defined at the bottom of '
                  '`taskcluster/taskgraph/optimize/__init__.py`.'
          }],
    ]

    def try_config(self, strategy, **kwargs):
        if strategy:
            if ':' not in strategy:
                strategy = "taskgraph.optimize:{}".format(strategy)

            return {
                'optimize-strategies': strategy,
            }


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


class WorkerOverrides(TryConfig):

    arguments = [
        [
            ["--worker-override"],
            {
                "action": "append",
                "dest": "worker_overrides",
                "help": (
                    "Override the worker pool used for a given taskgraph worker alias. "
                    "The argument should be `<alias>=<worker-pool>`. "
                    "Can be specified multiple times."
                ),
            },
        ],
        [
            ["--worker-suffix"],
            {
                "action": "append",
                "dest": "worker_suffixes",
                "help": (
                    "Override the worker pool used for a given taskgraph worker alias, "
                    "by appending a suffix to the work-pool. "
                    "The argument should be `<alias>=<suffix>`. "
                    "Can be specified multiple times."
                ),
            },
        ],
    ]

    def try_config(self, worker_overrides, worker_suffixes, **kwargs):
        from taskgraph.config import load_graph_config
        from taskgraph.util.workertypes import get_worker_type

        overrides = {}
        if worker_overrides:
            for override in worker_overrides:
                alias, worker_pool = override.split("=", 1)
                if alias in overrides:
                    print(
                        "Can't override worker alias {alias} more than once. "
                        "Already set to use {previous}, but also asked to use {new}.".format(
                            alias=alias, previous=overrides[alias], new=worker_pool
                        )
                    )
                    sys.exit(1)
                overrides[alias] = worker_pool

        if worker_suffixes:
            root = build.topsrcdir
            root = os.path.join(root, "taskcluster", "ci")
            graph_config = load_graph_config(root)
            for worker_suffix in worker_suffixes:
                alias, suffix = worker_suffix.split("=", 1)
                if alias in overrides:
                    print(
                        "Can't override worker alias {alias} more than once. "
                        "Already set to use {previous}, but also asked "
                        "to add suffix {suffix}.".format(
                            alias=alias, previous=overrides[alias], suffix=suffix
                        )
                    )
                    sys.exit(1)
                provisioner, worker_type = get_worker_type(
                    graph_config, alias, level="1", release_level="staging",
                )
                overrides[alias] = "{provisioner}/{worker_type}{suffix}".format(
                    provisioner=provisioner, worker_type=worker_type, suffix=suffix
                )

        if overrides:
            return {"worker-overrides": overrides}


all_task_configs = {
    'artifact': Artifact,
    'browsertime': Browsertime,
    'chemspill-prio': ChemspillPrio,
    'disable-pgo': DisablePgo,
    'env': Environment,
    'gecko-profile': GeckoProfile,
    'path': Path,
    'pernosco': Pernosco,
    'rebuild': Rebuild,
    'routes': Routes,
    'strategy': OptimizeStrategies,
    'worker-overrides': WorkerOverrides,
}
