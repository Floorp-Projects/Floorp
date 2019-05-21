# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os.path
import json
import time
from datetime import datetime

from mozbuild.util import ReadOnlyDict, memoize
from mozversioncontrol import get_repository_object

from . import GECKO
from .util.attributes import release_level


class ParameterMismatch(Exception):
    """Raised when a parameters.yml has extra or missing parameters."""


@memoize
def get_head_ref():
    return get_repository_object(GECKO).head_ref


def get_contents(path):
    with open(path, "r") as fh:
        contents = fh.readline().rstrip()
    return contents


def get_version(version_dir='browser/config'):
    return _get_version(version_dir, 'version_display.txt')


def get_app_version(version_dir='browser/config'):
    return _get_version(version_dir, 'version.txt')


def _get_version(version_dir, version_file):
    version_path = os.path.join(GECKO, version_dir, version_file)
    return get_contents(version_path)


# Please keep this list sorted and in sync with taskcluster/docs/parameters.rst
# Parameters are of the form: {name: default} or {name: lambda: default}
PARAMETERS = {
    'app_version': get_app_version(),
    'base_repository': 'https://hg.mozilla.org/mozilla-unified',
    'build_date': lambda: int(time.time()),
    'build_number': 1,
    'do_not_optimize': [],
    'existing_tasks': {},
    'filters': ['target_tasks_method'],
    'head_ref': get_head_ref,
    'head_repository': 'https://hg.mozilla.org/mozilla-central',
    'head_rev': get_head_ref,
    'hg_branch': 'default',
    'level': '3',
    'message': '',
    'moz_build_date': lambda: datetime.now().strftime("%Y%m%d%H%M%S"),
    'next_version': None,
    'optimize_target_tasks': True,
    'owner': 'nobody@mozilla.com',
    'project': 'mozilla-central',
    'pushdate': lambda: int(time.time()),
    'pushlog_id': '0',
    'phabricator_diff': None,
    'release_enable_emefree': False,
    'release_enable_partners': False,
    'release_eta': '',
    'release_history': {},
    'release_partners': None,
    'release_partner_config': None,
    'release_partner_build_number': 1,
    'release_type': 'nightly',
    'release_product': None,
    'required_signoffs': [],
    'signoff_urls': {},
    'target_tasks_method': 'default',
    'tasks_for': 'hg-push',
    'try_mode': None,
    'try_options': None,
    'try_task_config': None,
    'version': get_version(),
}

COMM_PARAMETERS = {
    'comm_base_repository': 'https://hg.mozilla.org/comm-central',
    'comm_head_ref': None,
    'comm_head_repository': 'https://hg.mozilla.org/comm-central',
    'comm_head_rev': None,
}


class Parameters(ReadOnlyDict):
    """An immutable dictionary with nicer KeyError messages on failure"""

    def __init__(self, strict=True, **kwargs):
        self.strict = strict

        if not self.strict:
            # apply defaults to missing parameters
            for name, default in PARAMETERS.items():
                if name not in kwargs:
                    if callable(default):
                        default = default()
                    kwargs[name] = default

            if set(kwargs) & set(COMM_PARAMETERS.keys()):
                for name, default in COMM_PARAMETERS.items():
                    if name not in kwargs:
                        if callable(default):
                            default = default()
                        kwargs[name] = default

        ReadOnlyDict.__init__(self, **kwargs)

    def check(self):
        names = set(self)
        valid = set(PARAMETERS.keys())
        valid_comm = set(COMM_PARAMETERS.keys())
        msg = []

        missing = valid - names
        if missing:
            msg.append("missing parameters: " + ", ".join(missing))

        extra = names - valid

        if extra & set(valid_comm):
            # If any comm_* parameters are specified, ensure all of them are specified.
            missing = valid_comm - extra
            if missing:
                msg.append("missing parameters: " + ", ".join(missing))
            extra = extra - valid_comm

        if extra and self.strict:
            msg.append("extra parameters: " + ", ".join(extra))

        if msg:
            raise ParameterMismatch("; ".join(msg))

    def __getitem__(self, k):
        if not (k in PARAMETERS.keys() or k in COMM_PARAMETERS.keys()):
            raise KeyError("no such parameter {!r}".format(k))
        try:
            return super(Parameters, self).__getitem__(k)
        except KeyError:
            raise KeyError("taskgraph parameter {!r} not found".format(k))

    def is_try(self):
        """
        Determine whether this graph is being built on a try project or for
        `mach try fuzzy`.
        """
        return 'try' in self['project'] or self['try_mode'] == 'try_select'

    def file_url(self, path):
        """
        Determine the VCS URL for viewing a file in the tree, suitable for
        viewing by a human.

        :param basestring path: The path, relative to the root of the repository.

        :return basestring: The URL displaying the given path.
        """
        if path.startswith('comm/'):
            path = path[len('comm/'):]
            repo = self['comm_head_repository']
            rev = self['comm_head_rev']
        else:
            repo = self['head_repository']
            rev = self['head_rev']

        return '{}/file/{}/{}'.format(repo, rev, path)

    def release_level(self):
        """
        Whether this is a staging release or not.

        :return six.text_type: One of "production" or "staging".
        """
        return release_level(self['project'])


def load_parameters_file(filename, strict=True, overrides=None, trust_domain=None):
    """
    Load parameters from a path, url, decision task-id or project.

    Examples:
        task-id=fdtgsD5DQUmAQZEaGMvQ4Q
        project=mozilla-central
    """
    import urllib
    from taskgraph.util.taskcluster import get_artifact_url, find_task_id
    from taskgraph.util import yaml

    if overrides is None:
        overrides = {}

    if not filename:
        return Parameters(strict=strict, **overrides)

    try:
        # reading parameters from a local parameters.yml file
        f = open(filename)
    except IOError:
        # fetching parameters.yml using task task-id, project or supplied url
        task_id = None
        if filename.startswith("task-id="):
            task_id = filename.split("=")[1]
        elif filename.startswith("project="):
            if trust_domain is None:
                raise ValueError(
                    "Can't specify parameters by project "
                    "if trust domain isn't supplied.",
                )
            index = "{trust_domain}.v2.{project}.latest.taskgraph.decision".format(
                trust_domain=trust_domain,
                project=filename.split("=")[1],
            )
            task_id = find_task_id(index)

        if task_id:
            filename = get_artifact_url(task_id, 'public/parameters.yml')
        f = urllib.urlopen(filename)

    if filename.endswith('.yml'):
        kwargs = yaml.load_stream(f)
    elif filename.endswith('.json'):
        kwargs = json.load(f)
    else:
        raise TypeError("Parameters file `{}` is not JSON or YAML".format(filename))

    kwargs.update(overrides)

    return Parameters(strict=strict, **kwargs)


def parameters_loader(filename, strict=True, overrides=None):
    def get_parameters(graph_config):
        parameters = load_parameters_file(
            filename,
            strict=strict,
            overrides=overrides,
            trust_domain=graph_config["trust-domain"],
        )
        parameters.check()
        return parameters
    return get_parameters
