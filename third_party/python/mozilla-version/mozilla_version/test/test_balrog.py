import pytest

from mozilla_version.balrog import BalrogReleaseName
from mozilla_version.errors import PatternNotMatchedError
from mozilla_version.gecko import FirefoxVersion


@pytest.mark.parametrize(
    'product, major_number, minor_number, patch_number, beta_number, build_number, is_nightly, \
is_aurora_or_devedition, is_esr, expected_output_string', ((
    'firefox', 32, 0, None, None, 1, False, False, False, 'firefox-32.0-build1'
), (
    'firefox', 32, 0, 1, None, 2, False, False, False, 'firefox-32.0.1-build2'
), (
    'firefox', 32, 0, None, 3, 4, False, False, False, 'firefox-32.0b3-build4'
), (
    'firefox', 32, 0, None, None, 5, True, False, False, 'firefox-32.0a1-build5'
), (
    'firefox', 32, 0, None, None, 6, False, True, False, 'firefox-32.0a2-build6'
), (
    'firefox', 32, 0, None, None, 7, False, False, True, 'firefox-32.0esr-build7'
), (
    'firefox', 32, 0, 1, None, 8, False, False, True, 'firefox-32.0.1esr-build8'
), (
    'devedition', 54, 0, None, 12, 1, False, False, False, 'devedition-54.0b12-build1'
), (
    'fennec', 32, 0, None, None, 1, False, False, False, 'fennec-32.0-build1'
), (
    'thunderbird', 32, 0, None, None, 1, False, False, False, 'thunderbird-32.0-build1'
)))
def test_balrog_release_name_constructor_and_str(
    product, major_number, minor_number, patch_number, beta_number, build_number, is_nightly,
    is_aurora_or_devedition, is_esr, expected_output_string
):
    assert str(BalrogReleaseName(product, FirefoxVersion(
        major_number=major_number,
        minor_number=minor_number,
        patch_number=patch_number,
        build_number=build_number,
        beta_number=beta_number,
        is_nightly=is_nightly,
        is_aurora_or_devedition=is_aurora_or_devedition,
        is_esr=is_esr
    ))) == expected_output_string


@pytest.mark.parametrize('product, major_number, minor_number, patch_number, beta_number, build_number, is_nightly, is_aurora_or_devedition, is_esr, ExpectedErrorType', ((
    ('nonexistingproduct', 32, 0, None, None, 1, False, False, False, PatternNotMatchedError),
    ('firefox', 32, 0, None, None, None, False, False, False, PatternNotMatchedError),
)))
def test_fail_balrog_release_constructor(product, major_number, minor_number, patch_number, beta_number, build_number, is_nightly, is_aurora_or_devedition, is_esr, ExpectedErrorType):
    with pytest.raises(ExpectedErrorType):
        BalrogReleaseName(product, FirefoxVersion(
            major_number=major_number,
            minor_number=minor_number,
            patch_number=patch_number,
            beta_number=beta_number,
            build_number=build_number,
            is_nightly=is_nightly,
            is_aurora_or_devedition=is_aurora_or_devedition,
            is_esr=is_esr
        ))


@pytest.mark.parametrize('string, expected_string', ((
    ('firefox-32.0-build1', 'firefox-32.0-build1'),
    ('firefox-32.0.1-build2', 'firefox-32.0.1-build2'),
    ('firefox-32.0b3-build4', 'firefox-32.0b3-build4'),
    ('firefox-32.0a1-build5', 'firefox-32.0a1-build5'),
    ('firefox-32.0a2-build6', 'firefox-32.0a2-build6'),
    ('firefox-32.0esr-build7', 'firefox-32.0esr-build7'),
    ('firefox-32.0.1esr-build8', 'firefox-32.0.1esr-build8'),

    ('firefox-32.0build1', 'firefox-32.0-build1'),
)))
def test_balrog_release_name_parse(string, expected_string):
    assert str(BalrogReleaseName.parse(string)) == expected_string


@pytest.mark.parametrize('string, ExpectedErrorType', (
    ('firefox-32.0', PatternNotMatchedError),

    ('firefox32.0-build1', PatternNotMatchedError),
    ('firefox32.0build1', PatternNotMatchedError),
    ('firefox-32.0--build1', PatternNotMatchedError),
    ('firefox-build1', PatternNotMatchedError),
    ('nonexistingproduct-32.0-build1', PatternNotMatchedError),

    ('firefox-32-build1', PatternNotMatchedError),
    ('firefox-32.b2-build1', PatternNotMatchedError),
    ('firefox-.1-build1', PatternNotMatchedError),
    ('firefox-32.0.0-build1', PatternNotMatchedError),
    ('firefox-32.2-build1', PatternNotMatchedError),
    ('firefox-32.02-build1', PatternNotMatchedError),
    ('firefox-32.0a0-build1', ValueError),
    ('firefox-32.0b0-build1', ValueError),
    ('firefox-32.0.1a1-build1', PatternNotMatchedError),
    ('firefox-32.0.1a2-build1', PatternNotMatchedError),
    ('firefox-32.0.1b2-build1', PatternNotMatchedError),
    ('firefox-32.0-build0', ValueError),
    ('firefox-32.0a1a2-build1', PatternNotMatchedError),
    ('firefox-32.0a1b2-build1', PatternNotMatchedError),
    ('firefox-32.0b2esr-build1', PatternNotMatchedError),
    ('firefox-32.0esrb2-build1', PatternNotMatchedError),
))
def test_firefox_version_raises_when_invalid_version_is_given(string, ExpectedErrorType):
    with pytest.raises(ExpectedErrorType):
        BalrogReleaseName.parse(string)


@pytest.mark.parametrize('previous, next', (
    ('firefox-32.0-build1', 'firefox-33.0-build1'),
    ('firefox-32.0-build1', 'firefox-32.1.0-build1'),
    ('firefox-32.0-build1', 'firefox-32.0.1-build1'),
    ('firefox-32.0-build1', 'firefox-32.0-build2'),

    ('firefox-32.0a1-build1', 'firefox-32.0-build1'),
    ('firefox-32.0a2-build1', 'firefox-32.0-build1'),
    ('firefox-32.0b1-build1', 'firefox-32.0-build1'),

    ('firefox-32.0.1-build1', 'firefox-33.0-build1'),
    ('firefox-32.0.1-build1', 'firefox-32.1.0-build1'),
    ('firefox-32.0.1-build1', 'firefox-32.0.2-build1'),
    ('firefox-32.0.1-build1', 'firefox-32.0.1-build2'),

    ('firefox-32.1.0-build1', 'firefox-33.0-build1'),
    ('firefox-32.1.0-build1', 'firefox-32.2.0-build1'),
    ('firefox-32.1.0-build1', 'firefox-32.1.1-build1'),
    ('firefox-32.1.0-build1', 'firefox-32.1.0-build2'),

    ('firefox-32.0b1-build1', 'firefox-33.0b1-build1'),
    ('firefox-32.0b1-build1', 'firefox-32.0b2-build1'),
    ('firefox-32.0b1-build1', 'firefox-32.0b1-build2'),

    ('firefox-2.0-build1', 'firefox-10.0-build1'),
    ('firefox-10.2.0-build1', 'firefox-10.10.0-build1'),
    ('firefox-10.0.2-build1', 'firefox-10.0.10-build1'),
    ('firefox-10.10.1-build1', 'firefox-10.10.10-build1'),
    ('firefox-10.0-build2', 'firefox-10.0-build10'),
    ('firefox-10.0b2-build1', 'firefox-10.0b10-build1'),
))
def test_balrog_release_implements_lt_operator(previous, next):
    assert BalrogReleaseName.parse(previous) < BalrogReleaseName.parse(next)


def test_fail_balrog_release_lt_operator():
    with pytest.raises(ValueError):
        assert BalrogReleaseName.parse('thunderbird-32.0-build1') < BalrogReleaseName.parse('Firefox-32.0-build2')


def test_balrog_release_implements_remaining_comparision_operators():
    assert BalrogReleaseName.parse('firefox-32.0-build1') == BalrogReleaseName.parse('firefox-32.0-build1')
    assert BalrogReleaseName.parse('firefox-32.0-build1') != BalrogReleaseName.parse('firefox-33.0-build1')

    assert BalrogReleaseName.parse('firefox-32.0-build1') <= BalrogReleaseName.parse('firefox-32.0-build1')
    assert BalrogReleaseName.parse('firefox-32.0-build1') <= BalrogReleaseName.parse('firefox-33.0-build1')

    assert BalrogReleaseName.parse('firefox-33.0-build1') >= BalrogReleaseName.parse('firefox-32.0-build1')
    assert BalrogReleaseName.parse('firefox-33.0-build1') >= BalrogReleaseName.parse('firefox-33.0-build1')

    assert BalrogReleaseName.parse('firefox-33.0-build1') > BalrogReleaseName.parse('firefox-32.0-build1')
    assert not BalrogReleaseName.parse('firefox-33.0-build1') > BalrogReleaseName.parse('firefox-33.0-build1')

    assert not BalrogReleaseName.parse('firefox-32.0-build1') < BalrogReleaseName.parse('firefox-32.0-build1')

    assert BalrogReleaseName.parse('firefox-33.0-build1') != BalrogReleaseName.parse('firefox-32.0-build1')

def test_balrog_release_hashable():
    """
    It is possible to hash `BalrogReleaseNmae`.
    """
    hash(BalrogReleaseName.parse('firefox-63.0-build1'))
