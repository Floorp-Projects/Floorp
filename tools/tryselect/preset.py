# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import ConfigParser
import os

from mozboot.util import get_state_dir


CONFIG_PATH = os.path.join(get_state_dir()[0], "autotry.ini")


def list_presets(section=None):
    config = ConfigParser.RawConfigParser()

    data = []
    if config.read([CONFIG_PATH]):
        sections = [section] if section else config.sections()
        for s in sections:
            try:
                data.extend(config.items(s))
            except ConfigParser.NoOptionError:
                pass

    if not data:
        print("No presets found")

    for name, value in data:
        print("%s: %s" % (name, value))


def load(name, section=None):
    config = ConfigParser.RawConfigParser()
    if not config.read([CONFIG_PATH]):
        return

    sections = [section] if section else config.sections()
    for s in sections:
        try:
            return config.get(s, name), s
        except (ConfigParser.NoSectionError, ConfigParser.NoOptionError):
            pass
    return None, None


def save(section, name, data):
    config = ConfigParser.RawConfigParser()
    config.read([CONFIG_PATH])

    if not config.has_section(section):
        config.add_section(section)

    config.set(section, name, data)

    with open(CONFIG_PATH, "w") as f:
        config.write(f)

    print('preset saved, run with: --preset={}'.format(name))
