import os
import shutil
from unittest import mock

import pytest

from mozperftest.environment import SYSTEM, TEST
from mozperftest.test.mochitest import MissingMochitestInformation
from mozperftest.tests.support import (
    EXAMPLE_MOCHITEST_TEST,
    get_running_env,
)


def running_env(**kw):
    return get_running_env(flavor="mochitest", **kw)


@mock.patch("mozperftest.test.mochitest.ON_TRY", new=False)
@mock.patch("mozperftest.utils.ON_TRY", new=False)
def test_mochitest_metrics(*mocked):
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_MOCHITEST_TEST)],
        mochitest_extra_args=[],
    )

    sys = env.layers[SYSTEM]
    mochitest = env.layers[TEST]

    with mock.patch("moztest.resolve.TestResolver") as test_resolver_mock, mock.patch(
        "mozperftest.test.functionaltestrunner.load_class_from_path"
    ) as load_class_path_mock, mock.patch(
        "mozperftest.test.functionaltestrunner.mozlog.formatters.MachFormatter.__new__"
    ) as formatter_mock, mock.patch(
        "mozperftest.test.mochitest.install_requirements_file"
    ):
        formatter_mock.return_value = lambda x: x

        def test_print(*args, **kwargs):
            log_processor = kwargs.get("custom_handler")
            log_processor.__call__('perfMetrics | { "fake": 0 }')
            return 0

        test_mock = mock.MagicMock()
        test_mock.test = test_print
        load_class_path_mock.return_value = test_mock

        test_resolver_mock.resolve_metadata.return_value = (1, 1)
        mach_cmd._spawn.return_value = test_resolver_mock
        try:
            with sys as s, mochitest as m:
                m(s(metadata))
        finally:
            shutil.rmtree(mach_cmd._mach_context.state_dir)

    res = metadata.get_results()
    assert len(res) == 1
    assert res[0]["name"] == "test_mochitest.html"
    results = res[0]["results"]

    assert results[0]["name"] == "fake"
    assert results[0]["values"] == [0]


@mock.patch(
    # This mock.patch actually patches the mochitest run_test_harness function
    "runtests.run_test_harness"
)
@mock.patch(
    # This mock.patch causes mochitest's runtests to be imported instead of
    # others in the remote_run
    "mochitest.runtests.run_test_harness",
    new=mock.MagicMock(),
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.mozlog.formatters.MachFormatter.__new__"
)
@mock.patch(
    "mozperftest.test.mochitest.install_requirements_file", new=mock.MagicMock()
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.load_class_from_path", new=mock.MagicMock()
)
@mock.patch("moztest.resolve.TestResolver", new=mock.MagicMock())
@mock.patch("mozperftest.test.mochitest.ON_TRY", new=True)
@mock.patch("mozperftest.utils.ON_TRY", new=True)
@mock.patch("mochitest.mochitest_options.MochitestArgumentParser", new=mock.MagicMock())
@mock.patch("manifestparser.TestManifest", new=mock.MagicMock())
def test_mochitest_ci_metrics(formatter_mock, run_test_harness_mock):
    if not os.getenv("MOZ_FETCHES_DIR"):
        os.environ["MOZ_FETCHES_DIR"] = "fake-path"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_MOCHITEST_TEST)],
        mochitest_extra_args=[],
        mochitest_manifest="fake.ini",
        mochitest_manifest_flavor="mocha",
    )

    system = env.layers[SYSTEM]
    mochitest = env.layers[TEST]

    formatter_mock.return_value = lambda x: x

    def test_print(*args, **kwargs):
        print('perfMetrics | { "fake": 0 }')
        return 0

    run_test_harness_mock.side_effect = test_print
    try:
        with system as s, mochitest as m:
            m(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    res = metadata.get_results()
    assert len(res) == 1
    assert res[0]["name"] == "test_mochitest.html"
    results = res[0]["results"]

    assert results[0]["name"] == "fake"
    assert results[0]["values"] == [0]


@mock.patch(
    # This mock.patch actually patches the mochitest run_test_harness function
    "runtests.run_test_harness",
    new=mock.MagicMock(),
)
@mock.patch(
    # This mock.patch causes mochitest's runtests to be imported instead of
    # others in the remote_run
    "mochitest.runtests.run_test_harness",
    new=mock.MagicMock(),
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.mozlog.formatters.MachFormatter.__new__",
    new=mock.MagicMock(),
)
@mock.patch(
    "mozperftest.test.mochitest.install_requirements_file", new=mock.MagicMock()
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.load_class_from_path", new=mock.MagicMock()
)
@mock.patch("moztest.resolve.TestResolver", new=mock.MagicMock())
@mock.patch("mozperftest.test.mochitest.ON_TRY", new=True)
@mock.patch("mozperftest.utils.ON_TRY", new=True)
@mock.patch("mochitest.mochitest_options.MochitestArgumentParser", new=mock.MagicMock())
@mock.patch("manifestparser.TestManifest", new=mock.MagicMock())
def test_mochitest_ci_metrics_missing_manifest():
    if not os.getenv("MOZ_FETCHES_DIR"):
        os.environ["MOZ_FETCHES_DIR"] = "fake-path"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_MOCHITEST_TEST)],
        mochitest_extra_args=[],
        mochitest_manifest_flavor="mocha",
    )

    system = env.layers[SYSTEM]
    mochitest = env.layers[TEST]

    try:
        with pytest.raises(MissingMochitestInformation) as exc:
            with system as s, mochitest as m:
                m(s(metadata))
        assert "manifest" in exc.value.args[0]
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    res = metadata.get_results()
    assert len(res) == 0


@mock.patch(
    # This mock.patch actually patches the mochitest run_test_harness function
    "runtests.run_test_harness",
    new=mock.MagicMock(),
)
@mock.patch(
    # This mock.patch causes mochitest's runtests to be imported instead of
    # others in the remote_run
    "mochitest.runtests.run_test_harness",
    new=mock.MagicMock(),
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.mozlog.formatters.MachFormatter.__new__",
    new=mock.MagicMock(),
)
@mock.patch(
    "mozperftest.test.mochitest.install_requirements_file", new=mock.MagicMock()
)
@mock.patch(
    "mozperftest.test.functionaltestrunner.load_class_from_path", new=mock.MagicMock()
)
@mock.patch("moztest.resolve.TestResolver", new=mock.MagicMock())
@mock.patch("mozperftest.test.mochitest.ON_TRY", new=True)
@mock.patch("mozperftest.utils.ON_TRY", new=True)
@mock.patch("mochitest.mochitest_options.MochitestArgumentParser", new=mock.MagicMock())
@mock.patch("manifestparser.TestManifest", new=mock.MagicMock())
def test_mochitest_ci_metrics_missing_flavor():
    if not os.getenv("MOZ_FETCHES_DIR"):
        os.environ["MOZ_FETCHES_DIR"] = "fake-path"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_MOCHITEST_TEST)],
        mochitest_extra_args=[],
        mochitest_manifest="fake.ini",
    )

    system = env.layers[SYSTEM]
    mochitest = env.layers[TEST]

    try:
        with pytest.raises(MissingMochitestInformation) as exc:
            with system as s, mochitest as m:
                m(s(metadata))
        assert "flavor" in exc.value.args[0]
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    res = metadata.get_results()
    assert len(res) == 0
