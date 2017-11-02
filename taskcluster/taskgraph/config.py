# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .util.schema import validate_schema, Schema
from voluptuous import Required

graph_config_schema = Schema({
    Required('treeherder'): {
        # Mapping of treeherder group symbols to descriptive names
        Required('group-names'): {basestring: basestring}
    }
})


def validate_graph_config(config):
    return validate_schema(graph_config_schema, config, "Invalid graph configuration:")
