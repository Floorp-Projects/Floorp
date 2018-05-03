# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import logging
import attr
import yaml
from mozpack import path

from .util.schema import validate_schema, Schema
from voluptuous import Required, Optional

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
    Required('treeherder'): {
        # Mapping of treeherder group symbols to descriptive names
        Required('group-names'): {basestring: basestring}
    },
    Required('index'): {
        Required('products'): [basestring],
    },
    Required('try'): {
        # We have a few platforms for which we want to do some "extra" builds, or at
        # least build-ish things.  Sort of.  Anyway, these other things are implemented
        # as different "platforms".  These do *not* automatically ride along with "-p
        # all"
        Required('ridealong-builds', default={}): {basestring: [basestring]},
    },
    Required('release-promotion'): {
        Required('products'): [basestring],
        Required('flavors'): {basestring: {
            Required('product'): basestring,
            Required('target-tasks-method'): basestring,
            Optional('release-type'): basestring,
            Optional('rebuild-kinds'): [basestring],
        }},
    },
    Required('scriptworker'): {
        # Prefix to add to scopes controlling scriptworkers
        Required('scope-prefix'): basestring,
        # Mapping of scriptworker types to scopes they accept
        Required('worker-types'): {basestring: [basestring]}
    },
    Required('partner'): {
        # Release config for partner repacks
        Required('release'): {basestring: basestring},
        # Staging config for partner repacks
        Required('staging'): {basestring: basestring},
    },
})


@attr.s(frozen=True)
class GraphConfig(object):
    _config = attr.ib()
    root_dir = attr.ib()

    def __getitem__(self, name):
        return self._config[name]

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
    return validate_schema(graph_config_schema, config, "Invalid graph configuration:")


def load_graph_config(root_dir):
    config_yml = os.path.join(root_dir, "config.yml")
    if not os.path.exists(config_yml):
        raise Exception("Couldn't find taskgraph configuration: {}".format(config_yml))

    logger.debug("loading config from `{}`".format(config_yml))
    with open(config_yml) as f:
        config = yaml.load(f)

    validate_graph_config(config)
    return GraphConfig(config=config, root_dir=root_dir)
