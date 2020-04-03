# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import logging
import sys
import attr
from six import text_type
from mozpack import path

from .util.python_path import find_object
from .util.schema import validate_schema, Schema, optionally_keyed_by
from voluptuous import Required, Optional, Any
from .util.yaml import load_yaml

logger = logging.getLogger(__name__)

graph_config_schema = Schema({
    # The trust-domain for this graph.
    # (See https://firefox-source-docs.mozilla.org/taskcluster/taskcluster/taskgraph.html#taskgraph-trust-domain)  # noqa
    Required('trust-domain'): text_type,
    # This specifes the prefix for repo parameters that refer to the project being built.
    # This selects between `head_rev` and `comm_head_rev` and related paramters.
    # (See http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#push-information  # noqa
    # and http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#comm-push-information)  # noqa
    Required('project-repo-param-prefix'): text_type,
    # This specifies the top level directory of the application being built.
    # ie. "browser/" for Firefox, "comm/mail/" for Thunderbird.
    Required('product-dir'): text_type,
    Required('treeherder'): {
        # Mapping of treeherder group symbols to descriptive names
        Required('group-names'): {text_type: text_type}
    },
    Required('index'): {
        Required('products'): [text_type]
    },
    Required('try'): {
        # We have a few platforms for which we want to do some "extra" builds, or at
        # least build-ish things.  Sort of.  Anyway, these other things are implemented
        # as different "platforms".  These do *not* automatically ride along with "-p
        # all"
        Required('ridealong-builds'): {text_type: [text_type]},
    },
    Required('release-promotion'): {
        Required('products'): [text_type],
        Required('flavors'): {text_type: {
            Required('product'): text_type,
            Required('target-tasks-method'): text_type,
            Optional('is-rc'): bool,
            Optional('rebuild-kinds'): [text_type],
            Optional('version-bump'): bool,
            Optional('partial-updates'): bool,
        }},
    },
    Required('merge-automation'): {
        Required('behaviors'): {text_type: {
            Optional('from-branch'): text_type,
            Required('to-branch'): text_type,
            Optional('from-repo'): text_type,
            Required('to-repo'): text_type,
            Required('version-files'): [
                {
                    Required('filename'): text_type,
                    Optional('new-suffix'): text_type,
                    Optional('version-bump'): Any('major', 'minor'),
                }
            ],
            Required('replacements'): [[text_type]],
            Required('merge-old-head'): bool,
            Optional('base-tag'): text_type,
            Optional('end-tag'): text_type,
        }},
    },
    Required('scriptworker'): {
        # Prefix to add to scopes controlling scriptworkers
        Required('scope-prefix'): text_type,
    },
    Required('task-priority'): optionally_keyed_by('project', Any(
        'highest',
        'very-high',
        'high',
        'medium',
        'low',
        'very-low',
        'lowest',
    )),
    Required('partner-urls'): {
        Required('release-partner-repack'):
            optionally_keyed_by('release-product', 'release-level', 'release-type',
                                Any(text_type, None)),
        Required('release-eme-free-repack'):
            optionally_keyed_by('release-product', 'release-level', 'release-type',
                                Any(text_type, None)),
    },
    Required('workers'): {
        Required('aliases'): {
            text_type: {
                Required('provisioner'): optionally_keyed_by('level', text_type),
                Required('implementation'): text_type,
                Required('os'): text_type,
                Required('worker-type'): optionally_keyed_by('level', 'release-level', text_type),
            }
        },
    },
    Required('mac-notarization'): {
        Required('mac-behavior'):
            optionally_keyed_by('project', 'shippable',
                                Any('mac_notarize', 'mac_geckodriver', 'mac_sign',
                                    'mac_sign_and_pkg')),
        Required('mac-entitlements'):
            optionally_keyed_by('platform', 'release-level', text_type),
    },
    Required("taskgraph"): {
        Optional(
            "register",
            description="Python function to call to register extensions.",
        ): text_type,
        Optional('decision-parameters'): text_type,
    },
})


@attr.s(frozen=True, cmp=False)
class GraphConfig(object):
    _config = attr.ib()
    root_dir = attr.ib()

    _PATH_MODIFIED = False

    def __getitem__(self, name):
        return self._config[name]

    def register(self):
        """
        Add the project's taskgraph directory to the python path, and register
        any extensions present.
        """
        if GraphConfig._PATH_MODIFIED:
            raise Exception("Can't register multiple directories on python path.")
        GraphConfig._PATH_MODIFIED = True
        sys.path.insert(0, os.path.dirname(self.root_dir))
        register_path = self['taskgraph'].get('register')
        if register_path:
            find_object(register_path)(self)

    @property
    def taskcluster_yml(self):
        if path.split(self.root_dir)[-2:] != ['taskcluster', 'ci']:
            raise Exception(
                "Not guessing path to `.taskcluster.yml`. "
                "Graph config in non-standard location."
            )
        return os.path.join(
            os.path.dirname(os.path.dirname(self.root_dir)),
            ".taskcluster.yml",
        )


def validate_graph_config(config):
    validate_schema(graph_config_schema, config, "Invalid graph configuration:")


def load_graph_config(root_dir):
    config_yml = os.path.join(root_dir, "config.yml")
    if not os.path.exists(config_yml):
        raise Exception("Couldn't find taskgraph configuration: {}".format(config_yml))

    logger.debug("loading config from `{}`".format(config_yml))
    config = load_yaml(config_yml)

    validate_graph_config(config)
    return GraphConfig(config=config, root_dir=root_dir)
