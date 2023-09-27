# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from importlib import import_module

CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))
PROJECT_DIR = os.path.realpath(
    os.path.join(CURRENT_DIR, "..", "..", "mobile", "android")
)
ANDROID_COMPONENTS_DIR = os.path.join(PROJECT_DIR, "android-components")
FOCUS_DIR = os.path.join(PROJECT_DIR, "focus-android")
FENIX_DIR = os.path.join(PROJECT_DIR, "fenix")


def register(graph_config):
    """
    Import all modules that are siblings of this one, triggering decorators in
    the process.
    """
    _import_modules(
        [
            "job",
            "parameters",
            "routes",
            "target_tasks",
            "util.group_by",
            "worker_types",
        ]
    )


def _import_modules(modules):
    for module in modules:
        import_module(f".{module}", package=__name__)
