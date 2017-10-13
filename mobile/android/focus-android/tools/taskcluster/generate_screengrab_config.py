# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script will read the Screengrabfile.template from the project
root directory and created the final Screengrabfile by injecting
the locales passed to this script.
"""

import os
import sys

# Read template
template_path = os.path.join(os.path.dirname(__file__), '../../Screengrabfile.template')
with open(template_path) as template_file:
	template = template_file.read()

# Read locales from arguments
locales = sys.argv[1:]

# Generate list for config file in format: locales ['a', 'b', 'c']
locales_config = 'locales [' + ', '.join(map(lambda x: "'%s'" % x, locales[:])) + ']'

# Write configuration
config_path = os.path.join(os.path.dirname(__file__), '../../Screengrabfile')
with open(config_path, 'w') as config_file:
	config_file = open(config_path, 'w')
	config_file.write(template)
	config_file.write("\n")
	config_file.write(locales_config)
	config_file.write("\n")

print("Wrote Screengrabfile file (%s): %s" % (", ".join(locales), config_path))
