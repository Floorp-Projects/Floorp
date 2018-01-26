# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script talks to the taskcluster secrets service to obtain the
Google Firebase service account token and write it to the .firebase_token 
file in the root directory.
"""

# This script needs following things done:
# Create secrets folder in taskcluster, and have access rights
# copy the json key file into the taskcluster secret
# set the correct path for secrets.get() call

import os
import taskcluster

# Get JSON data from taskcluster secrets service
secrets = taskcluster.Secrets({'baseUrl': 'http://taskcluster/secrets/v1'})
data = secrets.get('project/focus/firebase')

token_file_path = os.path.join(os.path.dirname(__file__), '../../.firebase_token.json')
with open(token_file_path, 'w') as token_file:
    token_file.write(data['secret'])

print("Imported google firebase token from secrets service")
