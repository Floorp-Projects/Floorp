# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

root = os.path.abspath(os.path.dirname(__file__))

manifest_all = os.path.join(root, 'manifest.ini')

manifest_functional = os.path.join(root, 'functional', 'manifest.ini')

manifest_update_direct = os.path.join(root, 'update', 'direct', 'manifest.ini')
manifest_update_fallback = os.path.join(root, 'update', 'fallback', 'manifest.ini')

resources = os.path.join(root, 'resources')
