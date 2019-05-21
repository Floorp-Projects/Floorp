# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import logging
import attr
from six import text_type
from mozpack import path

from .util.schema import validate_schema, Schema, optionally_keyed_by
from voluptuous import Required, Optional, Any
from .util.yaml import load_yaml

logger = logging.getLogger(__name__)

graph_config_schema = Schema({
    # The trust-domain for this graph.
    # (See https://firefox-source-docs.mozilla.org/taskcluster/taskcluster/taskgraph.html#taskgraph-trust-domain)  # noqa
    Required('trust-domain'): basestring,
    # This specifes the prefix for repo parameters that refer to the project being built.
    # This selects between `head_rev` and `comm_head_rev` and related paramters.
    # (See http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#push-information  # noqa
    # and http://firefox-source-docs.mozilla.org/taskcluster/taskcluster/parameters.html#comm-push-information)  # noqa
    Required('project-repo-param-prefix'): basestring,
    # This specifies the top level directory of the application being built.
    # ie. "browser/" for Firefox, "comm/mail/" for Thunderbird.
    Required('product-dir'): basestring,
    Required('treeherder'): {
        # Mapping of treeherder group symbols to descriptive names
        Required('group-names'): {basestring: basestring}
    },
    Required('index'): {
        Required('products'): [basestring]
    },
    Required('try'): {
        # We have a few platforms for which we want to do some "extra" builds, or at
        # least build-ish things.  Sort of.  Anyway, these other things are implemented
        # as different "platforms".  These do *not* automatically ride along with "-p
        # all"
        Required('ridealong-builds'): {basestring: [basestring]},
    },
    Required('release-promotion'): {
        Required('products'): [basestring],
        Required('flavors'): {basestring: {
            Required('product'): basestring,
            Required('target-tasks-method'): basestring,
            Optional('is-rc'): bool,
            Optional('rebuild-kinds'): [basestring],
            Optional('version-bump'): bool,
            Optional('partial-updates'): bool,
        }},
    },
    Required('scriptworker'): {
        # Prefix to add to scopes controlling scriptworkers
        Required('scope-prefix'): basestring,
        # Mapping of scriptworker types to scopes they accept
        Required('worker-types'): {basestring: [basestring]}
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
                                Any(basestring, None)),
        Required('release-eme-free-repack'):
            optionally_keyed_by('release-product', 'release-level', 'release-type',
                                Any(basestring, None)),
    },
    Required('version-directory'): optionally_keyed_by('release-product', 'android-release-type',
                                                       basestring),
    Required('workers'): {
        Required('aliases'): {
            text_type: {
                Required('provisioner'): text_type,
                Required('implementation'): text_type,
                Required('os'): text_type,
                Required('worker-type'): optionally_keyed_by('level', text_type),
            }
        },
    },
})


@attr.s(frozen=True, cmp=False)
class GraphConfig(object):
    _config = attr.ib()
    root_dir = attr.ib()

    def __getitem__(self, name):
        return self._config[name]

    def get(self, *args, **kwargs):
        return self._config.get(*args, **kwargs)

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
