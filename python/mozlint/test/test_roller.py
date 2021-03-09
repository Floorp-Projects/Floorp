# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform
import signal
import subprocess
import sys
import time

import mozunit
import pytest

from mozlint.errors import LintersNotConfigured, NoValidLinter
from mozlint.result import Issue, ResultSummary
from itertools import chain


here = os.path.abspath(os.path.dirname(__file__))


def test_roll_no_linters_configured(lint, files):
    with pytest.raises(LintersNotConfigured):
        lint.roll(files)


def test_roll_successful(lint, linters, files):
    lint.read(linters("string", "regex", "external"))

    result = lint.roll(files)
    assert len(result.issues) == 1
    assert result.failed == set([])

    path = list(result.issues.keys())[0]
    assert os.path.basename(path) == "foobar.js"

    errors = result.issues[path]
    assert isinstance(errors, list)
    assert len(errors) == 6

    container = errors[0]
    assert isinstance(container, Issue)
    assert container.rule == "no-foobar"


def test_roll_from_subdir(lint, linters):
    lint.read(linters("string", "regex", "external"))

    oldcwd = os.getcwd()
    try:
        os.chdir(os.path.join(lint.root, "files"))

        # Path relative to cwd works
        result = lint.roll("foobar.js")
        assert len(result.issues) == 1
        assert len(result.failed) == 0
        assert result.returncode == 1

        # Path relative to root doesn't work
        result = lint.roll(os.path.join("files", "foobar.js"))
        assert len(result.issues) == 0
        assert len(result.failed) == 0
        assert result.returncode == 0

        # Paths from vcs are always joined to root instead of cwd
        lint.mock_vcs([os.path.join("files", "foobar.js")])
        result = lint.roll(outgoing=True)
        assert len(result.issues) == 1
        assert len(result.failed) == 0
        assert result.returncode == 1

        result = lint.roll(workdir=True)
        assert len(result.issues) == 1
        assert len(result.failed) == 0
        assert result.returncode == 1

        result = lint.roll(rev='not public() and keyword("dummy revset expression")')
        assert len(result.issues) == 1
        assert len(result.failed) == 0
        assert result.returncode == 1
    finally:
        os.chdir(oldcwd)


def test_roll_catch_exception(lint, linters, files, capfd):
    lint.read(linters("raises"))

    lint.roll(files)  # assert not raises
    out, err = capfd.readouterr()
    assert "LintException" in err


def test_roll_with_global_excluded_path(lint, linters, files):
    lint.exclude = ["**/foobar.js"]
    lint.read(linters("string", "regex", "external"))
    result = lint.roll(files)

    assert len(result.issues) == 0
    assert result.failed == set([])


def test_roll_with_local_excluded_path(lint, linters, files):
    lint.read(linters("excludes"))
    result = lint.roll(files)

    assert "**/foobar.js" in lint.linters[0]["local_exclude"]
    assert len(result.issues) == 0
    assert result.failed == set([])


def test_roll_with_no_files_to_lint(lint, linters, capfd):
    lint.read(linters("string", "regex", "external"))
    lint.mock_vcs([])
    result = lint.roll([], workdir=True)
    assert isinstance(result, ResultSummary)
    assert len(result.issues) == 0
    assert len(result.failed) == 0

    out, err = capfd.readouterr()
    assert "warning: no files linted" in out


def test_roll_with_invalid_extension(lint, linters, filedir):
    lint.read(linters("external"))
    result = lint.roll(os.path.join(filedir, "foobar.py"))
    assert len(result.issues) == 0
    assert result.failed == set([])


def test_roll_with_failure_code(lint, linters, files):
    lint.read(linters("badreturncode"))

    result = lint.roll(files, num_procs=1)
    assert len(result.issues) == 0
    assert result.failed == set(["BadReturnCodeLinter"])


def test_roll_warnings(lint, linters, files):
    lint.read(linters("warning"))
    result = lint.roll(files)
    assert len(result.issues) == 0
    assert result.total_issues == 0
    assert len(result.suppressed_warnings) == 1
    assert result.total_suppressed_warnings == 2

    lint.lintargs["show_warnings"] = True
    result = lint.roll(files)
    assert len(result.issues) == 1
    assert result.total_issues == 2
    assert len(result.suppressed_warnings) == 0
    assert result.total_suppressed_warnings == 0


def fake_run_worker(config, paths, **lintargs):
    result = ResultSummary(lintargs["root"])
    result.issues["count"].append(1)
    return result


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="monkeypatch issues with multiprocessing on Windows",
)
@pytest.mark.parametrize("num_procs", [1, 4, 8, 16])
def test_number_of_jobs(monkeypatch, lint, linters, files, num_procs):
    monkeypatch.setattr(sys.modules[lint.__module__], "_run_worker", fake_run_worker)

    linters = linters("string", "regex", "external")
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=num_procs).issues["count"])

    if len(files) >= num_procs:
        assert num_jobs == num_procs * len(linters)
    else:
        assert num_jobs == len(files) * len(linters)


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="monkeypatch issues with multiprocessing on Windows",
)
@pytest.mark.parametrize("max_paths,expected_jobs", [(1, 12), (4, 6), (16, 6)])
def test_max_paths_per_job(monkeypatch, lint, linters, files, max_paths, expected_jobs):
    monkeypatch.setattr(sys.modules[lint.__module__], "_run_worker", fake_run_worker)

    files = files[:4]
    assert len(files) == 4

    linters = linters("string", "regex", "external")[:3]
    assert len(linters) == 3

    lint.MAX_PATHS_PER_JOB = max_paths
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=2).issues["count"])
    assert num_jobs == expected_jobs


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="monkeypatch issues with multiprocessing on Windows",
)
@pytest.mark.parametrize("num_procs", [1, 4, 8, 16])
def test_number_of_jobs_global(monkeypatch, lint, linters, files, num_procs):
    monkeypatch.setattr(sys.modules[lint.__module__], "_run_worker", fake_run_worker)

    linters = linters("global")
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=num_procs).issues["count"])

    assert num_jobs == 1


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="monkeypatch issues with multiprocessing on Windows",
)
@pytest.mark.parametrize("max_paths", [1, 4, 16])
def test_max_paths_per_job_global(monkeypatch, lint, linters, files, max_paths):
    monkeypatch.setattr(sys.modules[lint.__module__], "_run_worker", fake_run_worker)

    files = files[:4]
    assert len(files) == 4

    linters = linters("global")[:1]
    assert len(linters) == 1

    lint.MAX_PATHS_PER_JOB = max_paths
    lint.read(linters)
    num_jobs = len(lint.roll(files, num_procs=2).issues["count"])
    assert num_jobs == 1


@pytest.mark.skipif(
    platform.system() == "Windows",
    reason="signal.CTRL_C_EVENT isn't causing a KeyboardInterrupt on Windows",
)
def test_keyboard_interrupt():
    # We use two linters so we'll have two jobs. One (string.yml) will complete
    # quickly. The other (slow.yml) will run slowly.  This way the first worker
    # will be be stuck blocking on the ProcessPoolExecutor._call_queue when the
    # signal arrives and the other still be doing work.
    cmd = [sys.executable, "runcli.py", "-l=string", "-l=slow"]
    env = os.environ.copy()
    env["PYTHONPATH"] = os.pathsep.join(sys.path)
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=here,
        env=env,
        universal_newlines=True,
    )
    time.sleep(1)
    proc.send_signal(signal.SIGINT)

    out = proc.communicate()[0]
    print(out)
    assert "warning: not all files were linted" in out
    assert "2 problems" in out
    assert "Traceback" not in out


def test_support_files(lint, linters, filedir, monkeypatch, files):
    jobs = []

    # Replace the original _generate_jobs with a new one that simply
    # adds jobs to a list (and then doesn't return anything).
    orig_generate_jobs = lint._generate_jobs

    def fake_generate_jobs(*args, **kwargs):
        jobs.extend([job[1] for job in orig_generate_jobs(*args, **kwargs)])
        return []

    monkeypatch.setattr(lint, "_generate_jobs", fake_generate_jobs)

    linter_path = linters("support_files")[0]
    lint.read(linter_path)
    lint.root = filedir

    # Modified support files only lint entire root if --outgoing or --workdir
    # are used.
    path = os.path.join(filedir, "foobar.js")
    lint.mock_vcs([os.path.join(filedir, "foobar.py")])
    lint.roll(path)
    actual_files = sorted(chain(*jobs))
    assert actual_files == [path]

    expected_files = sorted(files)

    jobs = []
    lint.roll(path, workdir=True)
    actual_files = sorted(chain(*jobs))
    assert actual_files == expected_files

    jobs = []
    lint.roll(path, outgoing=True)
    actual_files = sorted(chain(*jobs))
    assert actual_files == expected_files

    jobs = []
    lint.roll(path, rev='draft() and keyword("dummy revset expression")')
    actual_files = sorted(chain(*jobs))
    assert actual_files == expected_files

    # Lint config file is implicitly added as a support file
    lint.mock_vcs([linter_path])
    jobs = []
    lint.roll(path, outgoing=True, workdir=True)
    actual_files = sorted(chain(*jobs))
    assert actual_files == expected_files


def test_setup(lint, linters, filedir, capfd):
    with pytest.raises(NoValidLinter):
        lint.setup()

    lint.read(linters("setup", "setupfailed", "setupraised"))
    lint.setup()
    out, err = capfd.readouterr()
    assert "setup passed" in out
    assert "setup failed" in out
    assert "setup raised" in out
    assert "error: problem with lint setup, skipping" in out
    assert lint.result.failed_setup == set(["SetupFailedLinter", "SetupRaisedLinter"])


if __name__ == "__main__":
    mozunit.main()
