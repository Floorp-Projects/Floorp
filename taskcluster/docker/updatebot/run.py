#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import sys

sys.path.append("/builds/worker/checkouts/gecko/third_party/python")
sys.path.append(".")

import os
import stat
import base64
import signal
import requests
import subprocess
import taskcluster

# Bump this number when you need to cause a commit for the job to re-run: 17

OPERATING_MODE = (
    "prod"
    if os.environ.get("GECKO_HEAD_REPOSITORY", "")
    == "https://hg.mozilla.org/mozilla-central"
    else "dev"
)

GECKO_DEV_PATH = "/builds/worker/checkouts/gecko"
DEV_PHAB_URL = "https://phabricator-dev.allizom.org/"
PROD_PHAB_URL = "https://phabricator.services.mozilla.com/"

phabricator_url = DEV_PHAB_URL if OPERATING_MODE == "dev" else PROD_PHAB_URL


def log(*args):
    print(*args)


def get_secret(name):
    secret = None
    if "TASK_ID" in os.environ:
        secrets_url = (
            "http://taskcluster/secrets/v1/secret/project/updatebot/"
            + ("3" if OPERATING_MODE == "prod" else "2")
            + "/"
            + name
        )
        res = requests.get(secrets_url)
        res.raise_for_status()
        secret = res.json()
    else:
        secrets = taskcluster.Secrets(taskcluster.optionsFromEnvironment())
        secret = secrets.get("project/updatebot/" + OPERATING_MODE + "/" + name)
    secret = secret["secret"] if "secret" in secret else None
    secret = secret["value"] if "value" in secret else None
    return secret


# Get TC Secrets =======================================
log("Operating mode is ", OPERATING_MODE)
log("Getting secrets...")
bugzilla_api_key = get_secret("bugzilla-api-key")
phabricator_token = get_secret("phabricator-token")
try_sshkey = get_secret("try-sshkey")
database_config = get_secret("database-password")
sentry_url = get_secret("sentry-url")
sql_proxy_config = get_secret("sql-proxy-config")

os.chdir("/builds/worker/updatebot")

# Update Updatebot =======================================
if OPERATING_MODE == "dev":
    """
    If we are in development mode, we will update from github.
    (This command will probably only work if we checkout a branch FWIW.)

    This allows us to iterate faster by committing to github and
    re-running the cron job on Taskcluster, without rebuilding the
    Docker image.

    However, this mechanism is bypassing the security feature we
    have in-tree, where upstream out-of-tree code is fixed at a known
    revision and cannot be changed without a commit to m-c.

    Therefore, we only do this in dev mode when running on try.
    """
    log("Performing git repo update...")
    r = subprocess.run(["git", "symbolic-ref", "-q", "HEAD"])
    if r.returncode == 0:
        # This indicates we are on a branch, and not a specific revision
        subprocess.check_call(["git", "pull", "origin"])

# Set Up SSH & Phabricator ==============================
log("Setting up ssh and phab keys...")
with open("id_rsa", "w") as sshkey:
    sshkey.write(try_sshkey)
os.chmod("id_rsa", stat.S_IRUSR | stat.S_IWUSR)

arcrc = open("/builds/worker/.arcrc", "w")
towrite = """
{
  "hosts": {
    "PHAB_URL_HERE": {
      "token": "TOKENHERE"
    }
  }
}
""".replace(
    "TOKENHERE", phabricator_token
).replace(
    "PHAB_URL_HERE", phabricator_url + "api/"
)
arcrc.write(towrite)
arcrc.close()
os.chmod("/builds/worker/.arcrc", stat.S_IRUSR | stat.S_IWUSR)

# Set up the Cloud SQL Proxy =============================
log("Setting up cloud_sql_proxy...")
os.chdir("/builds/worker/")
with open("sql-proxy-key", "w") as proxy_key_file:
    proxy_key_file.write(
        base64.b64decode(sql_proxy_config["key-value"]).decode("utf-8")
    )

instance_name = sql_proxy_config["instance-name"]
sql_proxy_command = (
    "./go/bin/cloud_sql_proxy -instances="
    + instance_name
    + "=tcp:3306 -credential_file=sql-proxy-key"
)
sql_proxy = subprocess.Popen(
    sql_proxy_command,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    shell=True,
    start_new_session=True,
)
try:
    (stdout, stderr) = sql_proxy.communicate(input=None, timeout=2)
    log("sql proxy stdout:", stdout.decode("utf-8"))
    log("sql proxy stderr:", stderr.decode("utf-8"))
except subprocess.TimeoutExpired:
    log("no sqlproxy output in 2 seconds, this means it probably didn't error.")

database_config["host"] = "127.0.0.1"

# Vendor =================================================
log("Getting Updatebot ready...")
os.chdir("/builds/worker/updatebot")
localconfig = {
    "General": {
        "env": OPERATING_MODE,
        "gecko-path": GECKO_DEV_PATH,
    },
    "Logging": {
        "local": True,
        "sentry": True,
        "sentry_config": {"url": sentry_url, "debug": False},
    },
    "Database": database_config,
    "Bugzilla": {
        "apikey": bugzilla_api_key,
    },
    "Taskcluster": {
        "url_treeherder": "https://treeherder.mozilla.org/",
        "url_taskcluster": "http://taskcluster/",
    },
}

log("Writing local config file")
config = open("localconfig.py", "w")
config.write("localconfig = " + str(localconfig))
config.close()

log("Running updatebot")
subprocess.check_call(["poetry", "run", "./automation.py"])

# Clean up ===============================================
log("Killing cloud_sql_proxy")
os.killpg(sql_proxy.pid, signal.SIGTERM)
