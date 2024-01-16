import importlib
import tempfile
from pathlib import Path
from unittest import mock

import mozunit
import requests

LINTER = "condprof-addons"


def linter_module_mocks(
    customizations_path=".", browsertime_fetches_path="browsertime.yml", **othermocks
):
    return mock.patch.multiple(
        LINTER,
        CUSTOMIZATIONS_PATH=Path(customizations_path),
        BROWSERTIME_FETCHES_PATH=Path(browsertime_fetches_path),
        **othermocks,
    )


def linter_class_mocks(**mocks):
    return mock.patch.multiple(
        f"{LINTER}.CondprofAddonsLinter",
        **mocks,
    )


# Sanity check (make sure linter message includes the xpi filename).
def test_get_missing_xpi_msg(lint, paths):
    condprof_addons = importlib.import_module("condprof-addons")
    with linter_class_mocks(
        get_firefox_addons_tar_names=mock.Mock(return_value=list()),
    ):
        instance = condprof_addons.CondprofAddonsLinter(
            topsrcdir=paths()[0], logger=mock.Mock()
        )
        assert instance.get_missing_xpi_msg("test.xpi").startswith(
            "test.xpi is missing"
        )


def test_xpi_missing_from_firefox_addons_tar(lint, paths):
    fixture_customizations = paths("with-missing-xpi.json")
    with linter_module_mocks(), linter_class_mocks(
        get_firefox_addons_tar_names=mock.Mock(return_value=list()),
    ):
        logger_mock = mock.Mock()
        lint(fixture_customizations, logger=logger_mock)
        assert logger_mock.lint_error.call_count == 1
        assert Path(fixture_customizations[0]).samefile(
            logger_mock.lint_error.call_args.kwargs["path"]
        )
        importlib.import_module("condprof-addons")
        assert "non-existing.xpi" in logger_mock.lint_error.call_args.args[0]


def test_xpi_all_found_in_firefox_addons_tar(lint, paths):
    get_tarnames_mock = mock.Mock(
        return_value=["an-extension.xpi", "another-extension.xpi"]
    )
    read_json_mock = mock.Mock(
        return_value={
            "addons": {
                "an-extension": "http://localhost/ext/an-extension.xpi",
                "another-extension": "http://localhost/ext/another-extension.xpi",
            }
        }
    )

    with linter_module_mocks(), linter_class_mocks(
        get_firefox_addons_tar_names=get_tarnames_mock, read_json=read_json_mock
    ):
        logger_mock = mock.Mock()
        # Compute a fake condprof customization path, the content is
        # going to be the read_json_mock.return_value and so the
        # fixture file does not actually exists.
        fixture_customizations = paths("fake-condprof-config.json")
        lint(
            fixture_customizations,
            logger=logger_mock,
            config={"include": paths(), "extensions": ["json", "yml"]},
        )
        assert read_json_mock.call_count == 1
        assert get_tarnames_mock.call_count == 1
        assert logger_mock.lint_error.call_count == 0


def test_lint_error_on_missing_or_invalid_firefoxaddons_fetch_task(
    lint,
    paths,
):
    read_json_mock = mock.Mock(return_value=dict())
    read_yaml_mock = mock.Mock(return_value=dict())
    # Verify that an explicit linter error is reported if the fetch task is not found.
    with linter_module_mocks(), linter_class_mocks(
        read_json=read_json_mock, read_yaml=read_yaml_mock
    ):
        logger_mock = mock.Mock()
        fixture_customizations = paths("fake-condprof-config.json")
        condprof_addons = importlib.import_module("condprof-addons")

        def assert_linter_error(yaml_mock_value, expected_msg):
            logger_mock.reset_mock()
            read_yaml_mock.return_value = yaml_mock_value
            lint(fixture_customizations, logger=logger_mock)
            assert logger_mock.lint_error.call_count == 1
            expected_path = condprof_addons.BROWSERTIME_FETCHES_PATH
            assert logger_mock.lint_error.call_args.kwargs["path"] == expected_path
            assert logger_mock.lint_error.call_args.args[0] == expected_msg

        # Mock a yaml file that is not including the expected firefox-addons fetch task.
        assert_linter_error(
            yaml_mock_value=dict(), expected_msg=condprof_addons.ERR_FETCH_TASK_MISSING
        )
        # Mock a yaml file where firefox-addons is missing the fetch attribute.
        assert_linter_error(
            yaml_mock_value={"firefox-addons": {}},
            expected_msg=condprof_addons.ERR_FETCH_TASK_MISSING,
        )
        # Mock a yaml file where firefox-addons add-prefix is missing.
        assert_linter_error(
            yaml_mock_value={"firefox-addons": {"fetch": {}}},
            expected_msg=condprof_addons.ERR_FETCH_TASK_ADDPREFIX,
        )
        # Mock a yaml file where firefox-addons add-prefix is invalid.
        assert_linter_error(
            yaml_mock_value={
                "firefox-addons": {"fetch": {"add-prefix": "invalid-subdir-name/"}}
            },
            expected_msg=condprof_addons.ERR_FETCH_TASK_ADDPREFIX,
        )


def test_get_xpi_list_from_fetch_dir(lint, paths):
    # Verify that when executed on the CI, the helper method looks for the xpi files
    # in the MOZ_FETCHES_DIR subdir where they are expected to be unpacked by the
    # fetch task.
    with linter_module_mocks(
        MOZ_AUTOMATION=1, MOZ_FETCHES_DIR=paths("fake-fetches-dir")[0]
    ):
        condprof_addons = importlib.import_module("condprof-addons")
        logger_mock = mock.Mock()
        Path(paths("browsertime.yml")[0])

        linter = condprof_addons.CondprofAddonsLinter(
            topsrcdir=paths()[0], logger=logger_mock
        )
        results = linter.tar_xpi_filenames

        results.sort()
        assert results == ["fake-ext-01.xpi", "fake-ext-02.xpi"]


def test_get_xpi_list_from_downloaded_tar(lint, paths):
    def mocked_download_tar(firefox_addons_tar_url, tar_tmp_path):
        tar_tmp_path.write_bytes(Path(paths("firefox-addons-fake.tar")[0]).read_bytes())

    download_firefox_addons_tar_mock = mock.Mock()
    download_firefox_addons_tar_mock.side_effect = mocked_download_tar

    # Verify that when executed locally on a developer machine, the tar archive is downloaded
    # and the list of xpi files included in it returned by the helper method.
    with tempfile.TemporaryDirectory() as tempdir, linter_module_mocks(
        MOZ_AUTOMATION=0,
        tempdir=tempdir,
    ), linter_class_mocks(
        download_firefox_addons_tar=download_firefox_addons_tar_mock,
    ):
        condprof_addons = importlib.import_module("condprof-addons")
        logger_mock = mock.Mock()
        Path(paths("browsertime.yml")[0])

        linter = condprof_addons.CondprofAddonsLinter(
            topsrcdir=paths()[0], logger=logger_mock
        )
        results = linter.tar_xpi_filenames
        assert len(results) > 0
        print("List of addons found in the downloaded file archive:", results)
        assert all(filename.endswith(".xpi") for filename in results)
        assert download_firefox_addons_tar_mock.call_count == 1


@mock.patch("requests.get")
def test_error_on_downloading_tar(requests_get_mock, lint, paths):
    # Verify that when executed locally and the tar archive fails to download
    # the linter does report an explicit linting error with the http error included.
    with tempfile.TemporaryDirectory() as tempdir, linter_module_mocks(
        MOZ_AUTOMATION=0, tempdir=tempdir
    ):
        condprof_addons = importlib.import_module("condprof-addons")
        logger_mock = mock.Mock()
        response_mock = mock.Mock()
        response_mock.raise_for_status.side_effect = requests.exceptions.HTTPError(
            "MOCK_ERROR"
        )
        requests_get_mock.return_value = response_mock
        Path(paths("browsertime.yml")[0])

        linter = condprof_addons.CondprofAddonsLinter(
            topsrcdir=paths()[0], logger=logger_mock
        )

        assert (
            logger_mock.lint_error.call_args.kwargs["path"]
            == condprof_addons.BROWSERTIME_FETCHES_PATH
        )
        assert (
            logger_mock.lint_error.call_args.args[0]
            == f"{condprof_addons.ERR_FETCH_TASK_ARCHIVE}, MOCK_ERROR"
        )
        assert requests_get_mock.call_count == 1
        assert len(linter.tar_xpi_filenames) == 0


@mock.patch("requests.get")
def test_error_on_opening_tar(requests_get_mock, lint, paths):
    # Verify that when executed locally and the tar archive fails to open
    # the linter does report an explicit linting error with the tarfile error included.
    with tempfile.TemporaryDirectory() as tempdir, linter_module_mocks(
        MOZ_AUTOMATION=0, tempdir=tempdir
    ):
        condprof_addons = importlib.import_module("condprof-addons")
        logger_mock = mock.Mock()
        response_mock = mock.Mock()
        response_mock.raise_for_status.return_value = None

        def mock_iter_content(chunk_size):
            yield b"fake tar content"
            yield b"expected to trigger tarfile.ReadError"

        response_mock.iter_content.side_effect = mock_iter_content
        requests_get_mock.return_value = response_mock
        Path(paths("browsertime.yml")[0])

        linter = condprof_addons.CondprofAddonsLinter(
            topsrcdir=paths()[0], logger=logger_mock
        )

        assert (
            logger_mock.lint_error.call_args.kwargs["path"]
            == condprof_addons.BROWSERTIME_FETCHES_PATH
        )
        actual_msg = logger_mock.lint_error.call_args.args[0]
        print("Got linter error message:", actual_msg)
        assert actual_msg.startswith(
            f"{condprof_addons.ERR_FETCH_TASK_ARCHIVE}, file could not be opened successfully"
        )
        assert requests_get_mock.call_count == 1
        assert len(linter.tar_xpi_filenames) == 0


def test_lint_all_customization_files_when_linting_browsertime_yml(
    lint,
    paths,
):
    get_tarnames_mock = mock.Mock(return_value=["an-extension.xpi"])
    read_json_mock = mock.Mock(
        return_value={
            "addons": {"an-extension": "http://localhost/ext/an-extension.xpi"}
        }
    )
    with linter_module_mocks(
        customizations_path="fake-customizations-dir",
    ), linter_class_mocks(
        get_firefox_addons_tar_names=get_tarnames_mock,
        read_json=read_json_mock,
    ):
        logger_mock = mock.Mock()
        importlib.import_module("condprof-addons")
        # When mozlint detects a change to the ci fetch browser.yml support file,
        # condprof-addons linter is called for the entire customizations dir path
        # and we expect that to be expanded to the list of the json customizations
        # files from that directory path.
        lint(paths("fake-customizations-dir"), logger=logger_mock)
        # Expect read_json_mock to be called once per each of the json files
        # found in the fixture dir.
        assert read_json_mock.call_count == 3
        assert get_tarnames_mock.call_count == 1
        assert logger_mock.lint_error.call_count == 0


if __name__ == "__main__":
    mozunit.main()
