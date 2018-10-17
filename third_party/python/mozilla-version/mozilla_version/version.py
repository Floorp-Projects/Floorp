"""Defines common characteristics of a version at Mozilla."""

from enum import Enum


class VersionType(Enum):
    """Enum that sorts types of versions (e.g.: nightly, beta, release, esr).

    Supports comparison. `ESR` is considered higher than `RELEASE` (even if they technically have
    the same codebase). For instance: 60.0.1 < 60.0.1esr but 61.0 > 60.0.1esr.
    This choice has a practical use case: if you have a list of Release and ESR version, you can
    easily extract one kind or the other thanks to the VersionType.

    Examples:
        .. code-block:: python

            assert VersionType.NIGHTLY == VersionType.NIGHTLY
            assert VersionType.ESR > VersionType.RELEASE

    """

    NIGHTLY = 1
    AURORA_OR_DEVEDITION = 2
    BETA = 3
    RELEASE = 4
    ESR = 5

    def __eq__(self, other):
        """Implement `==` operator."""
        return self.compare(other) == 0

    def __ne__(self, other):
        """Implement `!=` operator."""
        return self.compare(other) != 0

    def __lt__(self, other):
        """Implement `<` operator."""
        return self.compare(other) < 0

    def __le__(self, other):
        """Implement `<=` operator."""
        return self.compare(other) <= 0

    def __gt__(self, other):
        """Implement `>` operator."""
        return self.compare(other) > 0

    def __ge__(self, other):
        """Implement `>=` operator."""
        return self.compare(other) >= 0

    def compare(self, other):
        """Compare this `VersionType` with anotherself.

        Returns:
            0 if equal
            < 0 is this precedes the other
            > 0 if the other precedes this

        """
        return self.value - other.value
