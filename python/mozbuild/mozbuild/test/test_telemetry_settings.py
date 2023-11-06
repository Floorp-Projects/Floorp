# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest import mock
from unittest.mock import Mock

import mozunit
import pytest
import requests
from mach.config import ConfigSettings
from mach.decorators import SettingsProvider
from mach.settings import MachSettings
from mach.telemetry import (
    initialize_telemetry_setting,
    record_telemetry_settings,
    resolve_is_employee,
)


@SettingsProvider
class OtherSettings:
    config_settings = [("foo.bar", "int", "", 1), ("build.abc", "string", "", "")]


def record_enabled_telemetry(mozbuild_path, settings):
    record_telemetry_settings(settings, mozbuild_path, True)


@pytest.fixture
def settings():
    s = ConfigSettings()
    s.register_provider(MachSettings)
    s.register_provider(OtherSettings)
    return s


def load_settings_file(mozbuild_path, settings):
    settings.load_file(os.path.join(mozbuild_path, "machrc"))


def write_config(mozbuild_path, contents):
    with open(os.path.join(mozbuild_path, "machrc"), "w") as f:
        f.write(contents)


def test_nonexistent(tmpdir, settings):
    record_enabled_telemetry(tmpdir, settings)
    load_settings_file(tmpdir, settings)
    assert settings.mach_telemetry.is_enabled


def test_file_exists_no_build_section(tmpdir, settings):
    write_config(
        tmpdir,
        """[foo]
bar = 2
""",
    )
    record_enabled_telemetry(tmpdir, settings)
    load_settings_file(tmpdir, settings)
    assert settings.mach_telemetry.is_enabled
    assert settings.foo.bar == 2


def test_existing_build_section(tmpdir, settings):
    write_config(
        tmpdir,
        """[foo]
bar = 2

[build]
abc = xyz
""",
    )
    record_enabled_telemetry(tmpdir, settings)
    load_settings_file(tmpdir, settings)
    assert settings.mach_telemetry.is_enabled
    assert settings.build.abc == "xyz"
    assert settings.foo.bar == 2


def test_malformed_file(tmpdir, settings):
    """Ensure that a malformed config file doesn't cause breakage."""
    write_config(
        tmpdir,
        """[foo
bar = 1
""",
    )
    record_enabled_telemetry(tmpdir, settings)
    # Can't load_settings config, it will not have been written!


def _initialize_telemetry(settings, is_employee, contributor_prompt_response=None):
    with mock.patch(
        "mach.telemetry.resolve_is_employee", return_value=is_employee
    ), mock.patch(
        "mach.telemetry.prompt_telemetry_message_contributor",
        return_value=contributor_prompt_response,
    ) as prompt_mock, mock.patch(
        "subprocess.run", return_value=Mock(returncode=0)
    ), mock.patch(
        "mach.config.ConfigSettings"
    ):
        initialize_telemetry_setting(settings, "", "")
        return prompt_mock.call_count == 1


def test_initialize_new_contributor_deny_telemetry(settings):
    did_prompt = _initialize_telemetry(settings, False, False)
    assert did_prompt
    assert not settings.mach_telemetry.is_enabled
    assert settings.mach_telemetry.is_set_up
    assert settings.mach_telemetry.is_done_first_time_setup


def test_initialize_new_contributor_allow_telemetry(settings):
    did_prompt = _initialize_telemetry(settings, False, True)
    assert did_prompt
    assert settings.mach_telemetry.is_enabled
    assert settings.mach_telemetry.is_set_up
    assert settings.mach_telemetry.is_done_first_time_setup


def test_initialize_new_employee(settings):
    did_prompt = _initialize_telemetry(settings, True)
    assert not did_prompt
    assert settings.mach_telemetry.is_enabled
    assert settings.mach_telemetry.is_set_up
    assert settings.mach_telemetry.is_done_first_time_setup


def test_initialize_noop_when_telemetry_disabled_env(monkeypatch):
    monkeypatch.setenv("DISABLE_TELEMETRY", "1")
    with mock.patch("mach.telemetry.record_telemetry_settings") as record_mock:
        did_prompt = _initialize_telemetry(None, False)
        assert record_mock.call_count == 0
        assert not did_prompt


def test_initialize_noop_when_request_error(settings):
    with mock.patch(
        "mach.telemetry.resolve_is_employee",
        side_effect=requests.exceptions.RequestException("Unlucky"),
    ), mock.patch("mach.telemetry.record_telemetry_settings") as record_mock:
        initialize_telemetry_setting(None, None, None)
        assert record_mock.call_count == 0


def test_resolve_is_employee():
    def mock_and_run(is_employee_bugzilla, is_employee_vcs):
        with mock.patch(
            "mach.telemetry.resolve_is_employee_by_credentials",
            return_value=is_employee_bugzilla,
        ), mock.patch(
            "mach.telemetry.resolve_is_employee_by_vcs", return_value=is_employee_vcs
        ):
            return resolve_is_employee(None)

    assert not mock_and_run(False, False)
    assert not mock_and_run(False, True)
    assert not mock_and_run(False, None)
    assert mock_and_run(True, False)
    assert mock_and_run(True, True)
    assert mock_and_run(True, None)
    assert not mock_and_run(None, False)
    assert mock_and_run(None, True)


if __name__ == "__main__":
    mozunit.main()
