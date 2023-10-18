import mozunit
import pytest

from mozrelease.versions import (
    AncientMozillaVersion,
    ModernMozillaVersion,
    MozillaVersion,
)

ALL_VERSIONS = [  # Keep this sorted
    "3.0",
    "3.0.1",
    "3.0.2",
    "3.0.3",
    "3.0.4",
    "3.0.5",
    "3.0.6",
    "3.0.7",
    "3.0.8",
    "3.0.9",
    "3.0.10",
    "3.0.11",
    "3.0.12",
    "3.0.13",
    "3.0.14",
    "3.0.15",
    "3.0.16",
    "3.0.17",
    "3.0.18",
    "3.0.19",
    "3.1b1",
    "3.1b2",
    "3.1b3",
    "3.5b4",
    "3.5b99",
    "3.5rc1",
    "3.5rc2",
    "3.5rc3",
    "3.5",
    "3.5.1",
    "3.5.2",
    "3.5.3",
    "3.5.4",
    "3.5.5",
    "3.5.6",
    "3.5.7",
    "3.5.8",
    "3.5.9",
    "3.5.10",
    # ... Start skipping around...
    "4.0b9",
    "10.0.2esr",
    "10.0.3esr",
    "32.0",
    "49.0a1",
    "49.0a2",
    "59.0",
    "60.0",
    "60.0esr",
    "60.0.1esr",
    "60.1",
    "60.1esr",
    "61.0",
]


@pytest.fixture(
    scope="function",
    params=range(len(ALL_VERSIONS) - 1),
    ids=lambda x: "{}, {}".format(ALL_VERSIONS[x], ALL_VERSIONS[x + 1]),
)
def comparable_versions(request):
    index = request.param
    return ALL_VERSIONS[index], ALL_VERSIONS[index + 1]


@pytest.mark.parametrize("version", ALL_VERSIONS)
def test_versions_parseable(version):
    """Test that we can parse previously shipped versions.

    We only test 3.0 and up, since we never generate updates against
    versions that old."""
    assert MozillaVersion(version) is not None


def test_versions_compare_less(comparable_versions):
    """Test that versions properly compare in order."""
    smaller_version, larger_version = comparable_versions
    assert MozillaVersion(smaller_version) < MozillaVersion(larger_version)


def test_versions_compare_greater(comparable_versions):
    """Test that versions properly compare in order."""
    smaller_version, larger_version = comparable_versions
    assert MozillaVersion(larger_version) > MozillaVersion(smaller_version)


def test_ModernMozillaVersion():
    """Test properties specific to ModernMozillaVersion"""
    assert isinstance(MozillaVersion("1.2.4"), ModernMozillaVersion)
    assert isinstance(MozillaVersion("1.2.4rc3"), ModernMozillaVersion)
    assert MozillaVersion("1.2rc3") == MozillaVersion("1.2.0rc3")


def test_AncientMozillaVersion():
    """Test properties specific to AncientMozillaVersion"""
    assert isinstance(MozillaVersion("1.2.0.4"), AncientMozillaVersion)
    assert isinstance(MozillaVersion("1.2.0.4pre1"), AncientMozillaVersion)
    assert MozillaVersion("1.2pre1") == MozillaVersion("1.2.0pre1")
    assert MozillaVersion("1.2.0.4pre1") == MozillaVersion("1.2.4pre1")


@pytest.mark.parametrize("version", ALL_VERSIONS)
def test_versions_compare_equal(version):
    """Test that versions properly compare as equal through multiple passes."""
    assert MozillaVersion(version) == MozillaVersion(version)


if __name__ == "__main__":
    mozunit.main()
