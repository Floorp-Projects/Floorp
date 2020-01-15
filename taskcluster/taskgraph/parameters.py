# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import io
import os.path
import json
from datetime import datetime

from mozbuild.util import ReadOnlyDict, memoize
from mozversioncontrol import get_repository_object
from taskgraph.util.schema import validate_schema
from voluptuous import (
    ALLOW_EXTRA,
    Any,
    Inclusive,
    PREVENT_EXTRA,
    Required,
    Schema,
)

import six
from six import text_type

from . import GECKO
from .util.attributes import release_level


class ParameterMismatch(Exception):
    """Raised when a parameters.yml has extra or missing parameters."""


@memoize
def get_head_ref():
    return six.ensure_text(get_repository_object(GECKO).head_ref)


def get_contents(path):
    with io.open(path, "r") as fh:
        contents = fh.readline().rstrip()
    return contents


def get_version(product_dir='browser'):
    version_path = os.path.join(GECKO, product_dir, 'config',
                                'version_display.txt')
    return get_contents(version_path)


def get_app_version(product_dir='browser'):
    app_version_path = os.path.join(GECKO, product_dir, 'config',
                                    'version.txt')
    return get_contents(app_version_path)


base_schema = {
    Required('app_version'): text_type,
    Required('base_repository'): text_type,
    Required('build_date'): int,
    Required('build_number'): int,
    Inclusive('comm_base_repository', 'comm'): text_type,
    Inclusive('comm_head_ref', 'comm'): text_type,
    Inclusive('comm_head_repository', 'comm'): text_type,
    Inclusive('comm_head_rev', 'comm'): text_type,
    Required('do_not_optimize'): [text_type],
    Required('existing_tasks'): {text_type: text_type},
    Required('filters'): [text_type],
    Required('head_ref'): text_type,
    Required('head_repository'): text_type,
    Required('head_rev'): text_type,
    Required('hg_branch'): text_type,
    Required('level'): text_type,
    Required('message'): text_type,
    Required('moz_build_date'): text_type,
    Required('next_version'): Any(None, text_type),
    Required('optimize_target_tasks'): bool,
    Required('owner'): text_type,
    Required('phabricator_diff'): Any(None, text_type),
    Required('project'): text_type,
    Required('pushdate'): int,
    Required('pushlog_id'): text_type,
    Required('release_enable_emefree'): bool,
    Required('release_enable_partners'): bool,
    Required('release_eta'): Any(None, text_type),
    Required('release_history'): {text_type: dict},
    Required('release_partners'): Any(None, [text_type]),
    Required('release_partner_config'): Any(None, dict),
    Required('release_partner_build_number'): int,
    Required('release_type'): text_type,
    Required('release_product'): Any(None, text_type),
    Required('required_signoffs'): [text_type],
    Required('signoff_urls'): dict,
    Required('target_tasks_method'): text_type,
    Required('tasks_for'): text_type,
    Required('try_mode'): Any(None, text_type),
    Required('try_options'): Any(None, dict),
    Required('try_task_config'): dict,
    Required('version'): text_type,
}


COMM_PARAMETERS = [
    'comm_base_repository',
    'comm_head_ref',
    'comm_head_repository',
    'comm_head_rev',
]


class Parameters(ReadOnlyDict):
    """An immutable dictionary with nicer KeyError messages on failure"""

    def __init__(self, strict=True, **kwargs):
        self.strict = strict

        if not self.strict:
            # apply defaults to missing parameters
            kwargs = Parameters._fill_defaults(**kwargs)

        ReadOnlyDict.__init__(self, **kwargs)

    @staticmethod
    def _fill_defaults(**kwargs):
        now = datetime.utcnow()
        epoch = datetime.utcfromtimestamp(0)
        seconds_from_epoch = int((now - epoch).total_seconds())

        defaults = {
            'app_version': get_app_version(),
            'base_repository': 'https://hg.mozilla.org/mozilla-unified',
            'build_date': seconds_from_epoch,
            'build_number': 1,
            'do_not_optimize': [],
            'existing_tasks': {},
            'filters': ['target_tasks_method'],
            'head_ref': get_head_ref(),
            'head_repository': 'https://hg.mozilla.org/mozilla-central',
            'head_rev': get_head_ref(),
            'hg_branch': 'default',
            'level': '3',
            'message': '',
            'moz_build_date': six.ensure_text(now.strftime("%Y%m%d%H%M%S")),
            'next_version': None,
            'optimize_target_tasks': True,
            'owner': 'nobody@mozilla.com',
            'phabricator_diff': None,
            'project': 'mozilla-central',
            'pushdate': seconds_from_epoch,
            'pushlog_id': '0',
            'release_enable_emefree': False,
            'release_enable_partners': False,
            'release_eta': '',
            'release_history': {},
            'release_partners': [],
            'release_partner_config': None,
            'release_partner_build_number': 1,
            'release_product': None,
            'release_type': 'nightly',
            'required_signoffs': [],
            'signoff_urls': {},
            'target_tasks_method': 'default',
            'tasks_for': 'hg-push',
            'try_mode': None,
            'try_options': None,
            'try_task_config': {},
            'version': get_version(),
        }

        if set(COMM_PARAMETERS) & set(kwargs):
            defaults.update({
                'comm_base_repository': 'https://hg.mozilla.org/comm-central',
                'comm_head_repository': 'https://hg.mozilla.org/comm-central',
            })

        for name, default in defaults.items():
            if name not in kwargs:
                kwargs[name] = default

        return kwargs

    def check(self):
        schema = Schema(base_schema, extra=PREVENT_EXTRA if self.strict else ALLOW_EXTRA)
        validate_schema(schema, self.copy(), 'Invalid parameters:')

    def __getitem__(self, k):
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

    def file_url(self, path, pretty=False):
        """
        Determine the VCS URL for viewing a file in the tree, suitable for
        viewing by a human.

        :param text_type path: The path, relative to the root of the repository.
        :param bool pretty: Whether to return a link to a formatted version of the
            file, or the raw file version.
        :return text_type: The URL displaying the given path.
        """
        if path.startswith('comm/'):
            path = path[len('comm/'):]
            repo = self['comm_head_repository']
            rev = self['comm_head_rev']
        else:
            repo = self['head_repository']
            rev = self['head_rev']

        endpoint = 'file' if pretty else 'raw-file'
        return '{}/{}/{}/{}'.format(repo, endpoint, rev, path)

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
