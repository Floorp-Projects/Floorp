import pytest

from distutils.version import LooseVersion, StrictVersion

from mozilla_version.errors import PatternNotMatchedError
from mozilla_version.maven import MavenVersion

@pytest.mark.parametrize('major_number, minor_number, patch_number, is_snapshot, expected_output_string', ((
    32, 0, None, False, '32.0'
), (
    32, 0, 1, False, '32.0.1'
), (
    32, 0, None, True, '32.0-SNAPSHOT'
), (
    32, 0, 1, True, '32.0.1-SNAPSHOT'
)))
def test_maven_version_constructor_and_str(major_number, minor_number, patch_number, is_snapshot, expected_output_string):
    assert str(MavenVersion(
        major_number=major_number,
        minor_number=minor_number,
        patch_number=patch_number,
        is_snapshot=is_snapshot,
    )) == expected_output_string


def test_maven_version_constructor_minimum_kwargs():
    assert str(MavenVersion(32, 0)) == '32.0'
    assert str(MavenVersion(32, 0, 1)) == '32.0.1'
    assert str(MavenVersion(32, 1, 0)) == '32.1.0'
    assert str(MavenVersion(32, 1, 0, False)) == '32.1.0'
    assert str(MavenVersion(32, 1, 0, True)) == '32.1.0-SNAPSHOT'


@pytest.mark.parametrize('version_string, ExpectedErrorType', (
    ('32.0SNAPSHOT', PatternNotMatchedError),
    ('32.1.0SNAPSHOT', PatternNotMatchedError),
))
def test_maven_version_raises_when_invalid_version_is_given(version_string, ExpectedErrorType):
    with pytest.raises(ExpectedErrorType):
        MavenVersion.parse(version_string)


@pytest.mark.parametrize('previous, next', (
    ('32.0-SNAPSHOT', '32.0'),
    ('31.0', '32.0-SNAPSHOT'),
    ('32.0', '32.0.1-SNAPSHOT'),
    ('32.0.1-SNAPSHOT', '32.1.0'),
    ('32.0.1-SNAPSHOT', '33.0'),
))
def test_maven_version_implements_lt_operator(previous, next):
    assert MavenVersion.parse(previous) < MavenVersion.parse(next)


@pytest.mark.parametrize('previous, next', (
    ('32.0', '32.0-SNAPSHOT'),
    ('32.0-SNAPSHOT', '31.0'),
    ('32.0.1-SNAPSHOT', '32.0'),
    ('32.1.0', '32.0.1-SNAPSHOT'),
))
def test_maven_version_implements_gt_operator(previous, next):
    assert MavenVersion.parse(previous) > MavenVersion.parse(next)


@pytest.mark.parametrize('wrong_type', (
    32,
    32.0,
    ('32', '0', '1'),
    ['32', '0', '1'],
    LooseVersion('32.0'),
    StrictVersion('32.0'),
))
def test_base_version_raises_eq_operator(wrong_type):
    with pytest.raises(ValueError):
        assert MavenVersion.parse('32.0') == wrong_type
    # AttributeError is raised by LooseVersion and StrictVersion
    with pytest.raises((ValueError, AttributeError)):
        assert wrong_type == MavenVersion.parse('32.0')


def test_maven_version_implements_eq_operator():
    assert MavenVersion.parse('32.0-SNAPSHOT') == MavenVersion.parse('32.0-SNAPSHOT')
    # raw strings are also converted
    assert MavenVersion.parse('32.0-SNAPSHOT') == '32.0-SNAPSHOT'


def test_maven_version_hashable():
    hash(MavenVersion.parse('32.0.1'))
