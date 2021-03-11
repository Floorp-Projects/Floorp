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


"""
This is a sample mozperftest test that we use for testing
the verification process.
"""
SAMPLE_TEST = """
"use strict";

async function setUp(context) {
  context.log.info("setUp example!");
}

async function test(context, commands) {
  context.log.info("Test with setUp/tearDown example!");
  await commands.measure.start("https://www.sitespeed.io/");
  await commands.measure.start("https://www.mozilla.org/en-US/");
}

async function tearDown(context) {
  context.log.info("tearDown example!");
}

module.noexport = {};

module.exports = {
  setUp,
  tearDown,
  test,
  owner: "Performance Testing Team",
  name: "Example",
  description: "The description of the example test.",
  longDescription: `
  This is a longer description of the test perhaps including information
  about how it should be run locally or links to relevant information.
  `
};
"""


SAMPLE_CONFIG = """
name: mozperftest
manifest: None
static-only: False
suites:
    suite:
        description: "Performance tests from the 'suite' folder."
        tests:
            Example: ""
"""


DYNAMIC_SAMPLE_CONFIG = """
name: {}
manifest: None
static-only: False
suites:
    suite:
        description: "Performance tests from the 'suite' folder."
        tests:
            Example: "Performance test Example from suite."
    another_suite:
        description: "Performance tests from the 'another_suite' folder."
        tests:
            Example: "Performance test Example from another_suite."
"""


SAMPLE_INI = """
[Example]
test_url = Example_url
"""


@contextlib.contextmanager
def temp_file(name="temp", tempdir=None, content=None):
    if tempdir is None:
        tempdir = tempfile.mkdtemp()
    path = os.path.join(tempdir, name)
    if content is not None:
        with open(path, "w") as f:
            f.write(content)
    try:
        yield path
    finally:
        try:
            shutil.rmtree(tempdir)
        except FileNotFoundError:
            pass


@contextlib.contextmanager
def temp_dir():
    tempdir = tempfile.mkdtemp()
    try:
        yield tempdir
    finally:
        try:
            shutil.rmtree(tempdir)
        except FileNotFoundError:
            pass


def setup_sample_logger(logger, structured_logger, top_dir):
    from perfdocs.logger import PerfDocLogger

    PerfDocLogger.LOGGER = structured_logger
    PerfDocLogger.PATHS = ["perfdocs"]
    PerfDocLogger.TOP_DIR = top_dir

    import perfdocs.verifier as vf
    import perfdocs.gatherer as gt
    import perfdocs.generator as gn

    gt.logger = logger
    vf.logger = logger
    gn.logger = logger


@mock.patch("perfdocs.generator.Generator")
@mock.patch("perfdocs.verifier.Verifier")
@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_start_and_fail(verifier, generator, structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    with temp_file("bad", content="foo") as temp:
        run_perfdocs(config, logger=structured_logger, paths=[temp], generate=False)
        assert PerfDocsLoggerMock.LOGGER == structured_logger
        assert PerfDocsLoggerMock.PATHS == [temp]
        assert PerfDocsLoggerMock.FAILED

    assert verifier.call_count == 1
    assert mock.call().validate_tree() in verifier.mock_calls
    assert generator.call_count == 0


@mock.patch("perfdocs.generator.Generator")
@mock.patch("perfdocs.verifier.Verifier")
@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_start_and_pass(verifier, generator, structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    PerfDocsLoggerMock.FAILED = False
    with temp_file("bad", content="foo") as temp:
        run_perfdocs(config, logger=structured_logger, paths=[temp], generate=False)
        assert PerfDocsLoggerMock.LOGGER == structured_logger
        assert PerfDocsLoggerMock.PATHS == [temp]
        assert not PerfDocsLoggerMock.FAILED

    assert verifier.call_count == 1
    assert mock.call().validate_tree() in verifier.mock_calls
    assert generator.call_count == 1
    assert mock.call().generate_perfdocs() in generator.mock_calls


@mock.patch("perfdocs.logger.PerfDocLogger", new=PerfDocsLoggerMock)
def test_perfdocs_bad_paths(structured_logger, config, paths):
    from perfdocs.perfdocs import run_perfdocs

    with pytest.raises(Exception):
        run_perfdocs(config, logger=structured_logger, paths=["bad"], generate=False)


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_gatherer_fetch_perfdocs_tree(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.gatherer import Gatherer

    gatherer = Gatherer(top_dir)
    assert not gatherer._perfdocs_tree

    gatherer.fetch_perfdocs_tree()
    assert gatherer._perfdocs_tree

    expected = ["path", "yml", "rst", "static"]
    for i, key in enumerate(gatherer._perfdocs_tree[0].keys()):
        assert key == expected[i]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_gatherer_get_test_list(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.gatherer import Gatherer

    gatherer = Gatherer(top_dir)
    gatherer.fetch_perfdocs_tree()
    framework = gatherer.get_test_list(gatherer._perfdocs_tree[0])

    expected = ["name", "test_list", "yml_content", "yml_path"]
    for i, key in enumerate(sorted(framework.keys())):
        assert key == expected[i]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verification(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier.validate_tree()

    # Make sure that we had no warnings
    assert logger.warning.call_count == 0
    assert logger.log.call_count == 1
    assert len(logger.mock_calls) == 1


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_validate_yaml_pass(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    yaml_path = perfdocs_sample["config"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    valid = Verifier(top_dir).validate_yaml(yaml_path)

    assert valid


@mock.patch("perfdocs.verifier.jsonschema")
@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_invalid_yaml(
    logger, jsonschema, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    yaml_path = perfdocs_sample["config"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    jsonschema.validate = mock.Mock(side_effect=Exception("Schema/ValidationError"))
    verifier = Verifier("top_dir")
    valid = verifier.validate_yaml(yaml_path)

    expected = ("YAML ValidationError: Schema/ValidationError", yaml_path)
    args, _ = logger.warning.call_args

    assert logger.warning.call_count == 1
    assert args == expected
    assert not valid


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_validate_rst_pass(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    rst_path = perfdocs_sample["index"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    valid = Verifier(top_dir).validate_rst_content(rst_path)

    assert valid


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_invalid_rst(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    rst_path = perfdocs_sample["index"]
    setup_sample_logger(logger, structured_logger, top_dir)

    # Replace the target string to invalid Keyword for test
    with open(rst_path, "r") as file:
        filedata = file.read()

    filedata = filedata.replace("documentation", "Invalid Keyword")

    with open(rst_path, "w") as file:
        file.write(filedata)

    from perfdocs.verifier import Verifier

    verifier = Verifier("top_dir")
    valid = verifier.validate_rst_content(rst_path)

    expected = (
        "Cannot find a '{documentation}' entry in the given index file",
        rst_path,
    )
    args, _ = logger.warning.call_args

    assert logger.warning.call_count == 1
    assert args == expected
    assert not valid


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_validate_descriptions_pass(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier._check_framework_descriptions(verifier._gatherer.perfdocs_tree[0])

    assert logger.warning.call_count == 0
    assert logger.log.call_count == 1
    assert len(logger.mock_calls) == 1


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_not_existing_suite_in_test_list(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    manifest_path = perfdocs_sample["manifest"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    os.remove(manifest_path)
    verifier._check_framework_descriptions(verifier._gatherer.perfdocs_tree[0])

    expected = (
        "Could not find an existing suite for suite - bad suite name?",
        perfdocs_sample["config"],
    )
    args, _ = logger.warning.call_args

    assert logger.warning.call_count == 1
    assert args == expected


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_not_existing_tests_in_suites(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    with open(perfdocs_sample["config"], "r") as file:
        filedata = file.read()
        filedata = filedata.replace("Example", "DifferentName")
    with open(perfdocs_sample["config"], "w") as file:
        file.write(filedata)

    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier._check_framework_descriptions(verifier._gatherer.perfdocs_tree[0])

    expected = [
        "Could not find an existing test for DifferentName - bad test name?",
        "Could not find a test description for Example",
    ]

    assert logger.warning.call_count == 2
    for i, call in enumerate(logger.warning.call_args_list):
        args, _ = call
        assert args[0] == expected[i]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_missing_contents_in_suite(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    with open(perfdocs_sample["config"], "r") as file:
        filedata = file.read()
        filedata = filedata.replace("suite:", "InvalidSuite:")
    with open(perfdocs_sample["config"], "w") as file:
        file.write(filedata)

    from perfdocs.verifier import Verifier

    verifier = Verifier(top_dir)
    verifier._check_framework_descriptions(verifier._gatherer.perfdocs_tree[0])

    expected = (
        "Could not find an existing suite for InvalidSuite - bad suite name?",
        "Missing suite description for suite",
    )

    assert logger.warning.call_count == 2
    for i, call in enumerate(logger.warning.call_args_list):
        args, _ = call
        assert args[0] == expected[i]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_invalid_dir(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    verifier = Verifier("invalid_path")
    with pytest.raises(Exception) as exceinfo:
        verifier.validate_tree()

    assert str(exceinfo.value) == "No valid perfdocs directories found"


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_verifier_file_invalidation(
    logger, structured_logger, perfdocs_sample
):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.verifier import Verifier

    Verifier.validate_yaml = mock.Mock(return_value=False)
    verifier = Verifier(top_dir)
    with pytest.raises(Exception):
        verifier.validate_tree()

    # Check if "File validation error" log is called
    # and Called with a log inside perfdocs_tree().
    assert logger.log.call_count == 2
    assert len(logger.mock_calls) == 2


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_framework_gatherers(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    # Check to make sure that every single framework
    # gatherer that has been implemented produces a test list
    # in every suite that contains a test with an associated
    # manifest.
    from perfdocs.gatherer import frameworks

    for framework, gatherer in frameworks.items():
        with open(perfdocs_sample["config"], "w") as f:
            f.write(DYNAMIC_SAMPLE_CONFIG.format(framework))

        fg = gatherer(perfdocs_sample["config"], top_dir)
        if getattr(fg, "get_test_list", None) is None:
            # Skip framework gatherers that have not
            # implemented a method to build a test list.
            continue

        # Setup some framework-specific things here if needed
        if framework == "raptor":
            fg._manifest_path = perfdocs_sample["manifest"]
            fg._get_subtests_from_ini = mock.Mock()
            fg._get_subtests_from_ini.return_value = {
                "Example": perfdocs_sample["manifest"]
            }

        for suite, suitetests in fg.get_test_list().items():
            assert suite == "suite"
            for test, manifest in suitetests.items():
                assert test == "Example"
                assert manifest == perfdocs_sample["manifest"]


@mock.patch("perfdocs.logger.PerfDocLogger")
def test_perfdocs_framework_gatherers_urls(logger, structured_logger, perfdocs_sample):
    top_dir = perfdocs_sample["top_dir"]
    setup_sample_logger(logger, structured_logger, top_dir)

    from perfdocs.gatherer import frameworks
    from perfdocs.verifier import Verifier
    from perfdocs.generator import Generator
    from perfdocs.utils import read_yaml

    # This test is only for raptor
    gatherer = frameworks["raptor"]
    with open(perfdocs_sample["config"], "w") as f:
        f.write(DYNAMIC_SAMPLE_CONFIG.format("raptor"))

    fg = gatherer(perfdocs_sample["config_2"], top_dir)
    fg.get_suite_list = mock.Mock()
    fg.get_suite_list.return_value = {
        "suite": [perfdocs_sample["example1_manifest"]],
        "another_suite": [perfdocs_sample["example2_manifest"]],
    }

    v = Verifier(top_dir)
    gn = Generator(v, generate=True, workspace=top_dir)

    # Check to make sure that if a test is present under multiple
    # suties the urls are generated correctly for the test under
    # every suite
    for suite, suitetests in fg.get_test_list().items():
        url = fg._descriptions.get(suite)
        assert url is not None
        assert url[0]["name"] == "Example"
        assert url[0]["test_url"] == "Example_url"

    perfdocs_tree = gn._perfdocs_tree[0]
    yaml_content = read_yaml(
        os.path.join(os.path.join(perfdocs_tree["path"], perfdocs_tree["yml"]))
    )
    suites = yaml_content["suites"]

    # Check that the sections for each suite are generated correctly
    for suite_name, suite_details in suites.items():
        gn._verifier._gatherer = mock.Mock(framework_gatherers={"raptor": gatherer})
        section = gn._verifier._gatherer.framework_gatherers[
            "raptor"
        ].build_suite_section(fg, suite_name, suites.get(suite_name)["description"])
        assert suite_name.capitalize() == section[0]
        assert suite_name in section[2]

        tests = suites.get(suite_name).get("tests", {})
        for test_name in tests.keys():
            desc = gn._verifier._gatherer.framework_gatherers[
                "raptor"
            ].build_test_description(fg, test_name, tests[test_name], suite_name)
            assert suite_name in desc[0]
            assert test_name in desc[0]


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
