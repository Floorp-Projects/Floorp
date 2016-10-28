# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import json

engines = []

locale = sys.argv[2]
output_file = sys.argv[3]

output = open(output_file, 'w')

with open(sys.argv[1]) as f:
  searchinfo = json.load(f)

if locale in searchinfo["locales"]:
  output.write(json.dumps(searchinfo["locales"][locale]))
else:
  output.write(json.dumps(searchinfo["default"]))

output.close();
