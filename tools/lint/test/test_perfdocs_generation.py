from unittest import mock
import os
import pathlib

import mozunit

LINTER = "perfdocs"


def setup_sample_logger(logger, structured_logger, top_dir):
    from perfdocs.logger import PerfDocLogger

    PerfDocLogger.LOGGER = structured_logger
    PerfDocLogger.PATHS = ["perfdocs"]
    PerfDocLogger.TOP_DIR = top_dir

    import perfdocs.verifier as vf
    import perfdocs.gatherer as gt
    import perfdocs.generator as gn
    import perfdocs.utils as utils

    gt.logger = logger
    vf.logger = logger
    gn.logger = logger
    utils.logger = logger


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_generate_perfdocs_pass(
    logger, structured_logger, perfdocs_sample
):
    from test_perfdocs import temp_file

    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    templates_dir = pathlib.Path(top_dir, "tools", "lint", "perfdocs", "templates")
    templates_dir.mkdir(parents=True, exist_ok=True)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    with temp_file("index.rst", tempdir=templates_dir, content="{test_documentation}"):
        generator.generate_perfdocs()

    assert logger.warning.call_count == 0


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_needed_regeneration(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=False, workspace=top_dir)
    generator.generate_perfdocs()

    expected = "PerfDocs need to be regenerated."
    args, _ = logger.warning.call_args

    assert logger.warning.call_count == 1
    assert args[0] == expected


@mock.patch("perfdocs.generator.ON_TRY", new=True)
@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_needed_update(logger, structured_logger, perfdocs_sample):
    from test_perfdocs import temp_file

    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    templates_dir = pathlib.Path(top_dir, "tools", "lint", "perfdocs", "templates")
    templates_dir.mkdir(parents=True, exist_ok=True)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    # Initializing perfdocs
    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    with temp_file("index.rst", tempdir=templates_dir, content="{test_documentation}"):
        generator.generate_perfdocs()

        # Removed file for testing and run again
        generator._generate = False
        files = [f for f in os.listdir(generator.perfdocs_path)]
        for f in files:
            os.remove(str(pathlib.Path(generator.perfdocs_path, f)))

        generator.generate_perfdocs()

    expected = (
        "PerfDocs are outdated, run ./mach lint -l perfdocs --fix .` to update them. "
        "You can also apply the perfdocs.diff patch file produced from this "
        "reviewbot test to fix the issue."
    )
    args, _ = logger.warning.call_args

    assert logger.warning.call_count == 1
    assert args[0] == expected

    # Check to ensure a diff was produced
    assert logger.log.call_count == 6

    logs = [v[0][0] for v in logger.log.call_args_list]
    for failure_log in (
        "Some files are missing or are funny.",
        "Missing in existing docs: index.rst",
        "Missing in existing docs: mozperftest.rst",
    ):
        assert failure_log in logs


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_created_perfdocs(
    logger, structured_logger, perfdocs_sample
):
    from test_perfdocs import temp_file

    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    templates_dir = pathlib.Path(top_dir, "tools", "lint", "perfdocs", "templates")
    templates_dir.mkdir(parents=True, exist_ok=True)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    with temp_file("index.rst", tempdir=templates_dir, content="{test_documentation}"):
        perfdocs_tmpdir = generator._create_perfdocs()

    files = [f for f in os.listdir(perfdocs_tmpdir)]
    files.sort()
    expected_files = ["index.rst", "mozperftest.rst"]

    for i, file in enumerate(files):
        assert file == expected_files[i]

    with pathlib.Path(perfdocs_tmpdir, expected_files[0]).open() as f:
        filedata = f.readlines()
    assert "".join(filedata) == "  * :doc:`mozperftest`"


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_build_perfdocs(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    frameworks_info = generator.build_perfdocs_from_tree()

    expected = ["dynamic", "static"]

    for framework in sorted(frameworks_info.keys()):
        for i, framework_info in enumerate(frameworks_info[framework]):
            assert framework_info == expected[i]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_create_temp_dir(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    tmpdir = generator._create_temp_dir()

    assert pathlib.Path(tmpdir).is_dir()


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_create_temp_dir_fail(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    with mock.patch("perfdocs.generator.pathlib") as path_mock:
        path_mock.Path().mkdir.side_effect = OSError()
        path_mock.Path().is_dir.return_value = False
        tmpdir = generator._create_temp_dir()

    expected = "Error creating temp file: "
    args, _ = logger.critical.call_args

    assert not tmpdir
    assert logger.critical.call_count == 1
    assert args[0] == expected


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_save_perfdocs_pass(
    logger, structured_logger, perfdocs_sample
):
    from test_perfdocs import temp_file

    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    templates_dir = pathlib.Path(top_dir, "tools", "lint", "perfdocs", "templates")
    templates_dir.mkdir(parents=True, exist_ok=True)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)

    assert not generator.perfdocs_path.is_dir()

    with temp_file("index.rst", tempdir=templates_dir, content="{test_documentation}"):
        perfdocs_tmpdir = generator._create_perfdocs()

    generator._save_perfdocs(perfdocs_tmpdir)

    expected = ["index.rst", "mozperftest.rst"]
    files = [f for f in os.listdir(generator.perfdocs_path)]
    files.sort()

    for i, file in enumerate(files):
        assert file == expected[i]


@mock.patch("perfdocs.generator.shutil")
@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_generator_save_perfdocs_fail(
    logger, shutil, structured_logger, perfdocs_sample
):
    from test_perfdocs import temp_file

    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    templates_dir = pathlib.Path(top_dir, "tools", "lint", "perfdocs", "templates")
    templates_dir.mkdir(parents=True, exist_ok=True)

    from perfdocs.generator import Generator
    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    generator = Generator(verifier, generate=True, workspace=top_dir)
    with temp_file("index.rst", tempdir=templates_dir, content="{test_documentation}"):
        perfdocs_tmpdir = generator._create_perfdocs()

    shutil.copytree = mock.Mock(side_effect=Exception())
    generator._save_perfdocs(perfdocs_tmpdir)

    expected = "There was an error while saving the documentation: "
    args, _ = logger.critical.call_args

    assert logger.critical.call_count == 1
    assert args[0] == expected


if __name__ == "__main__":
    mozunit.main()
