# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import copy
import json
import subprocess

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.scriptworker import get_release_config
from taskgraph.util.schema import (
    resolve_keyed_by,
)

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def add_command(config, jobs):
    release_config = get_release_config(config)
    version = release_config["version"]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        command = [
            "cd", "/builds/worker/checkouts/gecko", "&&",
            "./mach", "python",
            "testing/mozharness/scripts/release/bouncer_check.py",
            "--version={}".format(version),
        ]
        job["run"]["command"] = command
        yield job


@transforms.add
def add_previous_versions(config, jobs):
    release_config = get_release_config(config)
    if not release_config.get("partial_versions"):
        for job in jobs:
            yield job
    else:
        extra_params = []
        for partial in release_config["partial_versions"].split(","):
            extra_params.append("--previous-version={}".format(partial.split("build")[0].strip()))

        for job in jobs:
            job = copy.deepcopy(job)  # don't overwrite dict values here
            job["run"]["command"].extend(extra_params)
            yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by project, etc."""
    fields = [
        "run.config",
        "run.extra-config",
    ]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(item=job, field=field, item_name=job['name'],
                             project=config.params['project'])

        for cfg in job["run"]["config"]:
            job["run"]["command"].extend(["--config", cfg])

        del job["run"]["config"]

        if 'extra-config' in job['run']:
            env = job['worker'].setdefault('env', {})
            env['EXTRA_MOZHARNESS_CONFIG'] = json.dumps(job['run']['extra-config'])
            del job["run"]["extra-config"]

        yield job


@transforms.add
def command_to_string(config, jobs):
    """Convert command to string to make it work properly with run-task"""
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        job["run"]["command"] = subprocess.list2cmdline(job["run"]["command"])
        yield job
