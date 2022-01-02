"""Defines common characteristics of a version at Mozilla."""

import attr
import re

from enum import Enum

from mozilla_version.errors import MissingFieldError, PatternNotMatchedError
from mozilla_version.parser import (
    get_value_matched_by_regex,
    does_regex_have_group,
    positive_int,
    positive_int_or_none
)


@attr.s(frozen=True, cmp=False, hash=True)
class BaseVersion(object):
    """Class that validates and handles general version numbers."""

    major_number = attr.ib(type=int, converter=positive_int)
    minor_number = attr.ib(type=int, converter=positive_int)
    patch_number = attr.ib(type=int, converter=positive_int_or_none, default=None)

    _MANDATORY_NUMBERS = ('major_number', 'minor_number')
    _OPTIONAL_NUMBERS = ('patch_number', )
    _ALL_NUMBERS = _MANDATORY_NUMBERS + _OPTIONAL_NUMBERS

    _VALID_ENOUGH_VERSION_PATTERN = re.compile(r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?$""", re.VERBOSE)

    @classmethod
    def parse(cls, version_string, regex_groups=()):
        """Construct an object representing a valid version number."""
        regex_matches = cls._VALID_ENOUGH_VERSION_PATTERN.match(version_string)

        if regex_matches is None:
            raise PatternNotMatchedError(version_string, cls._VALID_ENOUGH_VERSION_PATTERN)

        kwargs = {}

        for field in cls._MANDATORY_NUMBERS:
            kwargs[field] = get_value_matched_by_regex(field, regex_matches, version_string)
        for field in cls._OPTIONAL_NUMBERS:
            try:
                kwargs[field] = get_value_matched_by_regex(field, regex_matches, version_string)
            except MissingFieldError:
                pass

        for regex_group in regex_groups:
            kwargs[regex_group] = does_regex_have_group(regex_matches, regex_group)

        return cls(**kwargs)

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        semvers = [str(self.major_number), str(self.minor_number)]
        if self.patch_number is not None:
            semvers.append(str(self.patch_number))

        return '.'.join(semvers)

    def __eq__(self, other):
        """Implement `==` operator."""
        return self._compare(other) == 0

    def __ne__(self, other):
        """Implement `!=` operator."""
        return self._compare(other) != 0

    def __lt__(self, other):
        """Implement `<` operator."""
        return self._compare(other) < 0

    def __le__(self, other):
        """Implement `<=` operator."""
        return self._compare(other) <= 0

    def __gt__(self, other):
        """Implement `>` operator."""
        return self._compare(other) > 0

    def __ge__(self, other):
        """Implement `>=` operator."""
        return self._compare(other) >= 0

    def _compare(self, other):
        """Compare this release with another.

        Returns:
            0 if equal
            < 0 is this precedes the other
            > 0 if the other precedes this

        """
        if isinstance(other, str):
            other = BaseVersion.parse(other)
        elif not isinstance(other, BaseVersion):
            raise ValueError('Cannot compare "{}", type not supported!'.format(other))

        for field in ('major_number', 'minor_number', 'patch_number'):
            this_number = getattr(self, field)
            this_number = 0 if this_number is None else this_number
            other_number = getattr(other, field)
            other_number = 0 if other_number is None else other_number

            difference = this_number - other_number

            if difference != 0:
                return difference

        return 0


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

    __hash__ = Enum.__hash__
