# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import taskcluster

SECRET_NAME = 'project/android-components/publish'
TASKCLUSTER_BASE_URL = 'http://taskcluster/secrets/v1'

def fetch_publish_secrets(secret_name):
    """Fetch and return secrets from taskcluster's secret service"""
    secrets = taskcluster.Secrets({'baseUrl': TASKCLUSTER_BASE_URL})
    return secrets.get(secret_name)

def main():
    """Fetch the bintray user and api key from taskcluster's secret service
    and save it to local.properties in the project root directory.
    """
    data = fetch_publish_secrets(SECRET_NAME)

    properties_file_path = os.path.join(os.path.dirname(__file__), '../../../local.properties')
    with open(properties_file_path, 'w') as properties_file:
        properties_file.write("bintray.user=%s\n" % data['secret']['bintray_user'])
        properties_file.write("bintray.apikey=%s\n" % data['secret']['bintray_apikey'])

if __name__ == "__main__":
    main()
