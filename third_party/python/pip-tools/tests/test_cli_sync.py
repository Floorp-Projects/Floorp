import sys

import mock
import pytest

from .utils import invoke

from piptools.scripts.sync import cli


def test_run_as_module_sync():
    """piptools can be run as ``python -m piptools ...``."""

    status, output = invoke([sys.executable, "-m", "piptools", "sync", "--help"])

    # Should have run pip-compile successfully.
    output = output.decode("utf-8")
    assert output.startswith("Usage:")
    assert "Synchronize virtual environment with" in output
    assert status == 0


@mock.patch("piptools.sync.check_call")
def test_quiet_option(check_call, runner):
    """sync command can be run with `--quiet` or `-q` flag."""

    with open("requirements.txt", "w") as req_in:
        req_in.write("six==1.10.0")

    out = runner.invoke(cli, ["-q"])
    assert not out.stderr_bytes
    assert out.exit_code == 0

    # for every call to pip ensure the `-q` flag is set
    assert check_call.call_count == 2
    for call in check_call.call_args_list:
        assert "-q" in call[0][0]


@mock.patch("piptools.sync.check_call")
def test_quiet_option_when_up_to_date(check_call, runner):
    """
    Sync should output nothing when everything is up to date and quiet option is set.
    """
    with open("requirements.txt", "w"):
        pass

    with mock.patch("piptools.sync.diff", return_value=(set(), set())):
        out = runner.invoke(cli, ["-q"])

    assert not out.stderr_bytes
    assert out.exit_code == 0
    check_call.assert_not_called()


def test_no_requirements_file(runner):
    """
    It should raise an error if there are no input files
    or a requirements.txt file does not exist.
    """
    out = runner.invoke(cli)

    assert "No requirement files given" in out.stderr
    assert out.exit_code == 2


def test_input_files_with_dot_in_extension(runner):
    """
    It should raise an error if some of the input files have .in extension.
    """
    with open("requirements.in", "w") as req_in:
        req_in.write("six==1.10.0")

    out = runner.invoke(cli, ["requirements.in"])

    assert "ERROR: Some input files have the .in extension" in out.stderr
    assert out.exit_code == 2


def test_force_files_with_dot_in_extension(runner):
    """
    It should print a warning and sync anyway if some of the input files
    have .in extension.
    """

    with open("requirements.in", "w") as req_in:
        req_in.write("six==1.10.0")

    with mock.patch("piptools.sync.check_call"):
        out = runner.invoke(cli, ["requirements.in", "--force"])

    assert "WARNING: Some input files have the .in extension" in out.stderr
    assert out.exit_code == 0


def test_merge_error(runner):
    """
    Sync command should raise an error if there are merge errors.
    """
    with open("requirements.txt", "w") as req_in:
        req_in.write("six>1.10.0\n")

        # Add incompatible package
        req_in.write("six<1.10.0")

    with mock.patch("piptools.sync.check_call"):
        out = runner.invoke(cli, ["-n"])

    assert out.exit_code == 2
    assert "Incompatible requirements found" in out.stderr


@pytest.mark.parametrize(
    ("cli_flags", "expected_install_flags"),
    [
        (["--find-links", "./libs"], ["-f", "./libs"]),
        (["--no-index"], ["--no-index"]),
        (["--index-url", "https://example.com"], ["-i", "https://example.com"]),
        (
            ["--extra-index-url", "https://foo", "--extra-index-url", "https://bar"],
            ["--extra-index-url", "https://foo", "--extra-index-url", "https://bar"],
        ),
        (
            ["--trusted-host", "https://foo", "--trusted-host", "https://bar"],
            ["--trusted-host", "https://foo", "--trusted-host", "https://bar"],
        ),
        (
            ["--extra-index-url", "https://foo", "--trusted-host", "https://bar"],
            ["--extra-index-url", "https://foo", "--trusted-host", "https://bar"],
        ),
        (["--user"], ["--user"]),
        (["--cert", "foo.crt"], ["--cert", "foo.crt"]),
        (["--client-cert", "foo.pem"], ["--client-cert", "foo.pem"]),
    ],
)
@mock.patch("piptools.sync.check_call")
def test_pip_install_flags(check_call, cli_flags, expected_install_flags, runner):
    """
    Test the cli flags have to be passed to the pip install command.
    """
    with open("requirements.txt", "w") as req_in:
        req_in.write("six==1.10.0")

    runner.invoke(cli, cli_flags)

    call_args = [call[0][0] for call in check_call.call_args_list]
    assert [args[6:] for args in call_args if args[3] == "install"] == [
        expected_install_flags
    ]
