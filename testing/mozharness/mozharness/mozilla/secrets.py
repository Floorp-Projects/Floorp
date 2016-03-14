#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Support for fetching secrets from the secrets API
"""

import os
import mozharness
import urllib2
import json
from mozharness.base.log import ERROR


class SecretsMixin(object):

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
        """
        secret_files = self.config.get('secret_files', [])

        subst = {
            'scm-level': self.config.get('scm-level', 1),
        }

        for sf in secret_files:
            filename = sf['filename']
            secret_name = sf['secret_name'] % subst
            self.info("fetching {} from secret {}".format(filename, secret_name))

            # fetch from http://taskcluster, which points to the taskcluster proxy
            # within a taskcluster task.  Outside of that environment, do not
            # use this action.
            url = "http://taskcluster/secrets/v1/secret/" + secret_name
            res = urllib2.urlopen(url)
            if res.getcode() != 200:
                self.fatal("Error fetching from secrets API:" + res.read())

            secret = json.load(res)['secret']['content']

            open(filename, "w").write(filename)
