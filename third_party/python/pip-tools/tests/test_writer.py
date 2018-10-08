from pytest import fixture

from piptools._compat import FormatControl
from piptools.utils import comment
from piptools.writer import OutputWriter


@fixture
def writer():
    return OutputWriter(src_files=["src_file", "src_file2"], dst_file="dst_file",
                        dry_run=True,
                        emit_header=True, emit_index=True, emit_trusted_host=True,
                        annotate=True,
                        generate_hashes=False,
                        default_index_url=None, index_urls=[],
                        trusted_hosts=[],
                        format_control=FormatControl(set(), set()))


def test_format_requirement_annotation_editable(from_editable, writer):
    # Annotations are printed as comments at a fixed column
    ireq = from_editable('git+git://fake.org/x/y.git#egg=y')
    reverse_dependencies = {'y': ['xyz']}

    assert (writer._format_requirement(ireq,
                                       reverse_dependencies,
                                       primary_packages=[]) ==
            '-e git+git://fake.org/x/y.git#egg=y  ' + comment('# via xyz'))


def test_format_requirement_annotation(from_line, writer):
    ireq = from_line('test==1.2')
    reverse_dependencies = {'test': ['xyz']}

    assert (writer._format_requirement(ireq,
                                       reverse_dependencies,
                                       primary_packages=[]) ==
            'test==1.2                 ' + comment('# via xyz'))


def test_format_requirement_annotation_lower_case(from_line, writer):
    ireq = from_line('Test==1.2')
    reverse_dependencies = {'test': ['xyz']}

    assert (writer._format_requirement(ireq,
                                       reverse_dependencies,
                                       primary_packages=[]) ==
            'test==1.2                 ' + comment('# via xyz'))


def test_format_requirement_not_for_primary(from_line, writer):
    "Primary packages should not get annotated."
    ireq = from_line('test==1.2')
    reverse_dependencies = {'test': ['xyz']}

    assert (writer._format_requirement(ireq,
                                       reverse_dependencies,
                                       primary_packages=['test']) ==
            'test==1.2')


def test_format_requirement_not_for_primary_lower_case(from_line, writer):
    "Primary packages should not get annotated."
    ireq = from_line('Test==1.2')
    reverse_dependencies = {'test': ['xyz']}

    assert (writer._format_requirement(ireq,
                                       reverse_dependencies,
                                       primary_packages=['test']) ==
            'test==1.2')


def test_format_requirement_environment_marker(from_line, writer):
    "Environment markers should get passed through to output."
    ireq = from_line('test ; python_version == "2.7" and platform_python_implementation == "CPython"')
    reverse_dependencies = set()

    result = writer._format_requirement(
        ireq, reverse_dependencies, primary_packages=['test'],
        marker=ireq.markers)
    assert (result ==
            'test ; python_version == "2.7" and platform_python_implementation == "CPython"')


def test_iter_lines__unsafe_dependencies(from_line, writer):
    ireq = [from_line('test==1.2')]
    unsafe_req = [from_line('setuptools')]
    reverse_dependencies = {'test': ['xyz']}

    lines = writer._iter_lines(ireq,
                               unsafe_req,
                               reverse_dependencies,
                               ['test'],
                               {},
                               None)
    str_lines = []
    for line in lines:
        str_lines.append(line)
    assert comment('# The following packages are considered to be unsafe in a requirements file:') in str_lines
    assert comment('# setuptools') in str_lines
    assert 'test==1.2' in str_lines
