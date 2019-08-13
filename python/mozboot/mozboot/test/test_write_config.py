# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest

from mach.config import ConfigSettings
from mach.decorators import SettingsProvider
from mozboot.bootstrap import update_or_create_build_telemetry_config


# Duplicated from python/mozbuild/mozbuild/mach_commands.py because we can't
# actually import that module here.
@SettingsProvider
class TelemetrySettings():
    config_settings = [
        ('build.telemetry', 'boolean', """
Enable submission of build system telemetry.
        """.strip(), False),
    ]


@SettingsProvider
class OtherSettings():
    config_settings = [
        ('foo.bar', 'int', '', 1),
        ('build.abc', 'string', '', ''),
    ]


def read(path):
    s = ConfigSettings()
    s.register_provider(TelemetrySettings)
    s.register_provider(OtherSettings)
    s.load_file(path)
    return s


@pytest.fixture
def config_path(tmpdir):
    return unicode(tmpdir.join('machrc'))


@pytest.fixture
def write_config(config_path):
    def _config(contents):
        with open(config_path, 'wb') as f:
            f.write(contents)
    return _config


def test_nonexistent(config_path):
    update_or_create_build_telemetry_config(config_path)
    s = read(config_path)
    assert(s.build.telemetry)


def test_file_exists_no_build_section(config_path, write_config):
    write_config('''[foo]
bar = 2
''')
    update_or_create_build_telemetry_config(config_path)
    s = read(config_path)
    assert(s.build.telemetry)
    assert(s.foo.bar == 2)


def test_existing_build_section(config_path, write_config):
    write_config('''[foo]
bar = 2

[build]
abc = xyz
''')
    update_or_create_build_telemetry_config(config_path)
    s = read(config_path)
    assert(s.build.telemetry)
    assert(s.build.abc == 'xyz')
    assert(s.foo.bar == 2)


def test_malformed_file(config_path, write_config):
    """Ensure that a malformed config file doesn't cause breakage."""
    write_config('''[foo
bar = 1
''')
    assert(not update_or_create_build_telemetry_config(config_path))
    # Can't read config, it will not have been written!


if __name__ == '__main__':
    mozunit.main()
