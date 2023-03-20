# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import shutil
import subprocess
import tempfile
from contextlib import contextmanager
from pathlib import Path

import taskcluster
from appdirs import user_config_dir
from gecko_taskgraph import GECKO
from mach.base import FailedCommandError

logger = logging.getLogger(__name__)


TASK_TYPES = {
    "signing": ["linux-signing", "linux-signing-partial"],
    "beetmover": ["beetmover-candidates"],
    "bouncer": ["bouncer-submit"],
    "balrog": ["balrog-submit"],
    "tree": ["tree"],
}


def get_secret(secret):
    # use proxy if configured, otherwise use local credentials from env vars
    if "TASKCLUSTER_PROXY_URL" in os.environ:
        secrets_options = {"rootUrl": os.environ["TASKCLUSTER_PROXY_URL"]}
    else:
        secrets_options = taskcluster.optionsFromEnvironment()
    secrets = taskcluster.Secrets(secrets_options)
    return secrets.get(secret)["secret"]


@contextmanager
def configure_ssh(ssh_key_secret):
    if ssh_key_secret is None:
        yield

    # If we get here, we are running in automation.
    # We use a user hgrc, so that we also get the system-wide hgrc settings.
    hgrc = Path(user_config_dir("hg")) / "hgrc"
    if hgrc.exists():
        raise FailedCommandError(f"Not overwriting `{hgrc}`; cannot configure ssh.")

    try:
        ssh_key_dir = Path(tempfile.mkdtemp())

        ssh_key = get_secret(ssh_key_secret)
        ssh_key_file = ssh_key_dir / "id_rsa"
        ssh_key_file.write_text(ssh_key["ssh_privkey"])
        ssh_key_file.chmod(0o600)

        hgrc_content = (
            "[ui]\n"
            "username = trybld\n"
            "ssh = ssh -i {path} -l {user}\n".format(
                path=ssh_key_file, user=ssh_key["user"]
            )
        )
        hgrc.write_text(hgrc_content)

        yield
    finally:
        shutil.rmtree(str(ssh_key_dir))
        hgrc.unlink()


def push_canary(scriptworkers, addresses, ssh_key_secret):
    if ssh_key_secret and os.environ.get("MOZ_AUTOMATION", "0") != "1":
        # We make assumptions about the layout of the docker image
        # for creating the hgrc that we use for the key.
        raise FailedCommandError("Cannot use ssh-key-secret outside of automation.")

    # Collect the set of `mach try scriptworker` task sets to run.
    tasks = []
    for scriptworker in scriptworkers:
        worker_tasks = TASK_TYPES.get(scriptworker)
        if worker_tasks:
            logger.info("Running tasks for {}: {}".format(scriptworker, worker_tasks))
            tasks.extend(worker_tasks)
        else:
            logger.info("No tasks for {}.".format(scriptworker))

    mach = Path(GECKO) / "mach"
    base_command = [str(mach), "try", "scriptworker"]
    for address in addresses:
        base_command.extend(
            [
                "--route",
                "notify.email.{}.on-failed".format(address),
                "--route",
                "notify.email.{}.on-exception".format(address),
            ]
        )

    with configure_ssh(ssh_key_secret):
        env = os.environ.copy()
        for task in tasks:
            subprocess.check_call(base_command + [task], env=env)
