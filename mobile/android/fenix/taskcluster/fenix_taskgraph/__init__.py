# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from importlib import import_module


def register(graph_config):
    """
    Import all modules that are siblings of this one, triggering decorators in
    the process.
    """
    _import_modules([
        "job",
        "parameters",
        "release_promotion",
        "routes",
        "target_tasks",
        "worker_types",
    ])


def _import_modules(modules):
    for module in modules:
        import_module(".{}".format(module), package=__name__)
