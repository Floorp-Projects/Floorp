from pytest import fixture, mark, raises

from piptools._compat import FormatControl
from piptools.scripts.compile import cli
from piptools.utils import comment
from piptools.writer import (
    MESSAGE_UNHASHED_PACKAGE,
    MESSAGE_UNSAFE_PACKAGES,
    MESSAGE_UNSAFE_PACKAGES_UNPINNED,
    OutputWriter,
)


@fixture
def writer(tmpdir_cwd):
    with open("src_file", "w"), open("src_file2", "w"):
        pass

    cli_args = [
        "--dry-run",
        "--output-file",
        "requirements.txt",
        "src_file",
        "src_file2",
    ]

    with cli.make_context("pip-compile", cli_args) as ctx:
        writer = OutputWriter(
            src_files=["src_file", "src_file2"],
            dst_file=ctx.params["output_file"],
            click_ctx=ctx,
            dry_run=True,
            emit_header=True,
            emit_index=True,
            emit_trusted_host=True,
            annotate=True,
            generate_hashes=False,
            default_index_url=None,
            index_urls=[],
            trusted_hosts=[],
            format_control=FormatControl(set(), set()),
            allow_unsafe=False,
            find_links=[],
        )
        yield writer


def test_format_requirement_annotation_editable(from_editable, writer):
    # Annotations are printed as comments at a fixed column
    ireq = from_editable("git+git://fake.org/x/y.git#egg=y")
    reverse_dependencies = {"y": ["xyz"]}

    assert writer._format_requirement(
        ireq, reverse_dependencies, primary_packages=[]
    ) == "-e git+git://fake.org/x/y.git#egg=y  " + comment("# via xyz")


def test_format_requirement_annotation(from_line, writer):
    ireq = from_line("test==1.2")
    reverse_dependencies = {"test": ["xyz"]}

    assert writer._format_requirement(
        ireq, reverse_dependencies, primary_packages=[]
    ) == "test==1.2                 " + comment("# via xyz")


def test_format_requirement_annotation_lower_case(from_line, writer):
    ireq = from_line("Test==1.2")
    reverse_dependencies = {"test": ["xyz"]}

    assert writer._format_requirement(
        ireq, reverse_dependencies, primary_packages=[]
    ) == "test==1.2                 " + comment("# via xyz")


def test_format_requirement_not_for_primary(from_line, writer):
    "Primary packages should not get annotated."
    ireq = from_line("test==1.2")
    reverse_dependencies = {"test": ["xyz"]}

    assert (
        writer._format_requirement(
            ireq, reverse_dependencies, primary_packages=["test"]
        )
        == "test==1.2"
    )


def test_format_requirement_not_for_primary_lower_case(from_line, writer):
    "Primary packages should not get annotated."
    ireq = from_line("Test==1.2")
    reverse_dependencies = {"test": ["xyz"]}

    assert (
        writer._format_requirement(
            ireq, reverse_dependencies, primary_packages=["test"]
        )
        == "test==1.2"
    )


def test_format_requirement_environment_marker(from_line, writer):
    "Environment markers should get passed through to output."
    ireq = from_line(
        'test ; python_version == "2.7" and platform_python_implementation == "CPython"'
    )
    reverse_dependencies = set()

    result = writer._format_requirement(
        ireq, reverse_dependencies, primary_packages=["test"], marker=ireq.markers
    )
    assert (
        result == 'test ; python_version == "2.7" and '
        'platform_python_implementation == "CPython"'
    )


@mark.parametrize(("allow_unsafe",), [(True,), (False,)])
def test_iter_lines__unsafe_dependencies(writer, from_line, allow_unsafe):
    writer.allow_unsafe = allow_unsafe
    output = "\n".join(
        writer._iter_lines([from_line("test==1.2")], [from_line("setuptools")])
    )
    assert (
        "\n".join(
            [
                "test==1.2",
                "",
                MESSAGE_UNSAFE_PACKAGES,
                "setuptools" if allow_unsafe else comment("# setuptools"),
            ]
        )
        in output
    )


def test_iter_lines__unsafe_with_hashes(writer, from_line):
    writer.allow_unsafe = False
    ireqs = [from_line("test==1.2")]
    unsafe_ireqs = [from_line("setuptools")]
    hashes = {ireqs[0]: {"FAKEHASH"}, unsafe_ireqs[0]: set()}
    output = "\n".join(writer._iter_lines(ireqs, unsafe_ireqs, hashes=hashes))
    assert (
        "\n".join(
            [
                "test==1.2 \\",
                "    --hash=FAKEHASH",
                "",
                MESSAGE_UNSAFE_PACKAGES_UNPINNED,
                comment("# setuptools"),
            ]
        )
        in output
    )


def test_iter_lines__hash_missing(writer, from_line):
    ireqs = [from_line("test==1.2"), from_line("file:///example/#egg=example")]
    hashes = {ireqs[0]: {"FAKEHASH"}, ireqs[1]: set()}
    output = "\n".join(writer._iter_lines(ireqs, hashes=hashes))
    assert (
        "\n".join(
            [MESSAGE_UNHASHED_PACKAGE, "file:///example/#egg=example", "test==1.2"]
        )
        in output
    )


def test_write_header(writer):
    expected = map(
        comment,
        [
            "#",
            "# This file is autogenerated by pip-compile",
            "# To update, run:",
            "#",
            "#    pip-compile --output-file={} src_file src_file2".format(
                writer.click_ctx.params["output_file"].name
            ),
            "#",
        ],
    )
    assert list(writer.write_header()) == list(expected)


def test_write_header_custom_compile_command(writer, monkeypatch):
    monkeypatch.setenv("CUSTOM_COMPILE_COMMAND", "./pipcompilewrapper")
    expected = map(
        comment,
        [
            "#",
            "# This file is autogenerated by pip-compile",
            "# To update, run:",
            "#",
            "#    ./pipcompilewrapper",
            "#",
        ],
    )
    assert list(writer.write_header()) == list(expected)


def test_write_header_no_emit_header(writer):
    """
    There should not be headers if emit_header is False
    """
    writer.emit_header = False

    with raises(StopIteration):
        next(writer.write_header())


def test_write_format_controls(writer):
    """
    Tests --no-binary/--only-binary options.
    """

    writer.format_control = FormatControl(
        no_binary=["psycopg2", "click"], only_binary=["pytz", "django"]
    )
    lines = list(writer.write_format_controls())

    assert "--no-binary psycopg2" in lines
    assert "--no-binary click" in lines

    assert "--only-binary pytz" in lines
    assert "--only-binary django" in lines


@mark.parametrize(
    ("index_urls", "expected_lines"),
    (
        # No urls - no options
        [[], []],
        # Single URL should be index-url
        [["https://index-url.com"], ["--index-url https://index-url.com"]],
        # First URL should be index-url, the others should be extra-index-url
        [
            [
                "https://index-url1.com",
                "https://index-url2.com",
                "https://index-url3.com",
            ],
            [
                "--index-url https://index-url1.com",
                "--extra-index-url https://index-url2.com",
                "--extra-index-url https://index-url3.com",
            ],
        ],
        # If a first URL equals to the default URL, the the index url must not be set
        # and the others should be extra-index-url
        [
            [
                "https://default-index-url.com",
                "https://index-url1.com",
                "https://index-url2.com",
            ],
            [
                "--extra-index-url https://index-url1.com",
                "--extra-index-url https://index-url2.com",
            ],
        ],
        # Ignore URLs equal to the default index-url
        # (note: the previous case is exception)
        [
            [
                "https://index-url1.com",
                "https://default-index-url.com",
                "https://index-url2.com",
            ],
            [
                "--index-url https://index-url1.com",
                "--extra-index-url https://index-url2.com",
            ],
        ],
        # Ignore URLs equal to the default index-url
        [["https://default-index-url.com", "https://default-index-url.com"], []],
        # All URLs must be deduplicated
        [
            [
                "https://index-url1.com",
                "https://index-url1.com",
                "https://index-url2.com",
            ],
            [
                "--index-url https://index-url1.com",
                "--extra-index-url https://index-url2.com",
            ],
        ],
    ),
)
def test_write_index_options(writer, index_urls, expected_lines):
    """
    Test write_index_options method.
    """
    writer.index_urls = index_urls
    writer.default_index_url = "https://default-index-url.com"
    assert list(writer.write_index_options()) == expected_lines


def test_write_index_options_no_emit_index(writer):
    """
    There should not be --index-url/--extra-index-url options if emit_index is False.
    """
    writer.emit_index = False
    with raises(StopIteration):
        next(writer.write_index_options())


@mark.parametrize(
    "find_links, expected_lines",
    (
        [[], []],
        [["./foo"], ["--find-links ./foo"]],
        [["./foo", "./bar"], ["--find-links ./foo", "--find-links ./bar"]],
    ),
)
def test_write_find_links(writer, find_links, expected_lines):
    """
    Test write_find_links method.
    """
    writer.find_links = find_links
    assert list(writer.write_find_links()) == expected_lines
