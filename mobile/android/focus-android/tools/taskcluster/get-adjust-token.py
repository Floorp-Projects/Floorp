# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script talks to the taskcluster secrets service to obtain the
Adjust token and write it to the .adjust_token file in the root
directory.
"""

import os
import taskcluster

# Get JSON data from taskcluster secrets service
secrets = taskcluster.Secrets({'baseUrl': 'http://taskcluster/secrets/v1'})
data = secrets.get('project/focus/tokens')

token_file_path = os.path.join(os.path.dirname(__file__), '../../.adjust_token')
with open(token_file_path, 'w') as token_file:
	token_file.write(data['secret']['adjustToken'])

print("Imported adjust token from secrets service")
