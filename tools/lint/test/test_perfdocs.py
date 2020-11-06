import contextlib
import mock
import os
import pytest
import shutil
import tempfile

import mozunit

LINTER = "perfdocs"


class PerfDocsLoggerMock:
    LOGGER = None
    PATHS = []
    FAILED = True


@contextlib.contextmanager
def temp_file(name="temp", content=None):
    tempdir = tempfile.mkdtemp()
    path = os.path.join(tempdir, name)
    if content is not None:
        with open(path, "w") as f:
            f.write(content)
    try:
        yield path
    finally:
        shutil.rmtree(tempdir)


@mock.patch("perfdocs.generator.Generator")
@mock.patch("perfdocs.verifier.Verifier")
@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_start_and_fail(generator, verifier, structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    with temp_file("bad", "foo") as temp:
        run_perfdocs(config, logger=structured_logger, paths=[temp], generate=False)
        assert PerfDocsLoggerMock.LOGGER == structured_logger
        assert PerfDocsLoggerMock.PATHS == [temp]
        assert PerfDocsLoggerMock.FAILED

    assert verifier.validate_tree.assert_called_once()
    assert generator.generate_perfdocs.assert_not_called()


@mock.patch("perfdocs.generator.Generator")
@mock.patch("perfdocs.verifier.Verifier")
@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_start_and_pass(generator, verifier, structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    PerfDocsLoggerMock.FAILED = False
    with temp_file("bad", "foo") as temp:
        run_perfdocs(config, logger=structured_logger, paths=[temp], generate=False)
        assert PerfDocsLoggerMock.LOGGER == structured_logger
        assert PerfDocsLoggerMock.PATHS == [temp]
        assert not PerfDocsLoggerMock.FAILED

    assert verifier.validate_tree.assert_called_once()
    assert generator.generate_perfdocs.assert_called_once()


@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_bad_paths(structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    with pytest.raises(Exception):
        run_perfdocs(config, logger=structured_logger, paths=["bad"], generate=False)


def test_perfdocs_logger_failure(config, paths):
    from perfdocs.logger import PerfDocLogger

    PerfDocLogger.LOGGER = None
    with pytest.raises(Exception):
        PerfDocLogger()

    PerfDocLogger.PATHS = []
    with pytest.raises(Exception):
        PerfDocLogger()


if __name__ == "__main__":
    mozunit.main()
