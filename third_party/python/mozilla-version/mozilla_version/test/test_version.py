import pytest

from mozilla_version.version import VersionType


@pytest.mark.parametrize('previous, next', (
    (VersionType.NIGHTLY, VersionType.AURORA_OR_DEVEDITION),
    (VersionType.NIGHTLY, VersionType.BETA),
    (VersionType.NIGHTLY, VersionType.RELEASE),
    (VersionType.NIGHTLY, VersionType.ESR),

    (VersionType.AURORA_OR_DEVEDITION, VersionType.BETA),
    (VersionType.AURORA_OR_DEVEDITION, VersionType.RELEASE),
    (VersionType.AURORA_OR_DEVEDITION, VersionType.ESR),

    (VersionType.BETA, VersionType.RELEASE),
    (VersionType.BETA, VersionType.ESR),

    (VersionType.RELEASE, VersionType.ESR),
))
def test_version_type_implements_lt_operator(previous, next):
    assert previous < next


@pytest.mark.parametrize('first, second', (
    (VersionType.NIGHTLY, VersionType.NIGHTLY),
    (VersionType.AURORA_OR_DEVEDITION, VersionType.AURORA_OR_DEVEDITION),
    (VersionType.BETA, VersionType.BETA),
    (VersionType.RELEASE, VersionType.RELEASE),
    (VersionType.ESR, VersionType.ESR),
))
def test_version_type_implements_eq_operator(first, second):
    assert first == second


def test_version_type_implements_remaining_comparision_operators():
    assert VersionType.NIGHTLY <= VersionType.NIGHTLY
    assert VersionType.NIGHTLY <= VersionType.BETA

    assert VersionType.NIGHTLY >= VersionType.NIGHTLY
    assert VersionType.BETA >= VersionType.NIGHTLY

    assert not VersionType.NIGHTLY > VersionType.NIGHTLY
    assert VersionType.BETA > VersionType.NIGHTLY

    assert not VersionType.BETA < VersionType.NIGHTLY

    assert VersionType.NIGHTLY != VersionType.BETA


def test_version_type_compare():
    assert VersionType.NIGHTLY.compare(VersionType.NIGHTLY) == 0
    assert VersionType.NIGHTLY.compare(VersionType.BETA) < 0
    assert VersionType.BETA.compare(VersionType.NIGHTLY) > 0
