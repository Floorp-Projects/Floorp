#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Support for fetching secrets from the secrets API
"""

from __future__ import absolute_import
import os
import six
from six.moves import urllib
import json


class SecretsMixin(object):
    def _fetch_secret(self, secret_name):
        self.info("fetching secret {} from API".format(secret_name))
        # fetch from TASKCLUSTER_PROXY_URL, which points to the taskcluster proxy
        # within a taskcluster task.  Outside of that environment, do not
        # use this action.
        proxy = os.environ.get("TASKCLUSTER_PROXY_URL", "http://taskcluster")
        proxy = proxy.rstrip("/")
        url = proxy + "/secrets/v1/secret/" + secret_name
        res = urllib.request.urlopen(url)
        if res.getcode() != 200:
            self.fatal("Error fetching from secrets API:" + res.read())

        return json.loads(six.ensure_str(res.read()))["secret"]["content"]

    def get_secrets(self):
        """
        Get the secrets specified by the `secret_files` configuration.  This is
        a list of dictionaries, one for each secret.  The `secret_name` key
        names the key in the TaskCluster secrets API to fetch (see
        http://docs.taskcluster.net/services/secrets/).  It can contain
        %-substitutions based on the `subst` dictionary below.

        Since secrets must be JSON objects, the `content` property of the
        secret is used as the value to be written to disk.

        The `filename` key in the dictionary gives the filename to which the
        secret should be written.

        The optional `min_scm_level` key gives a minimum SCM level at which
        this secret is required.  For lower levels, the value of the 'default`
        key or the contents of the file specified by `default-file` is used, or
        no secret is written.

        The optional 'mode' key allows a mode change (chmod) after the file is written
        """
        dirs = self.query_abs_dirs()
        secret_files = self.config.get("secret_files", [])

        scm_level = int(os.environ.get("MOZ_SCM_LEVEL", "1"))
        subst = {
            "scm-level": scm_level,
        }

        for sf in secret_files:
            filename = os.path.abspath(sf["filename"])
            secret_name = sf["secret_name"] % subst
            min_scm_level = sf.get("min_scm_level", 0)
            if scm_level < min_scm_level:
                if "default" in sf:
                    self.info("Using default value for " + filename)
                    secret = sf["default"]
                elif "default-file" in sf:
                    default_path = sf["default-file"].format(**dirs)
                    with open(default_path, "r") as f:
                        secret = f.read()
                else:
                    self.info("No default for secret; not writing " + filename)
                    continue
            else:
                secret = self._fetch_secret(secret_name)

            open(filename, "w").write(secret)

            if sf.get("mode"):
                os.chmod(filename, sf["mode"])
