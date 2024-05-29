"""Defines common characteristics of a version at Mozilla."""

import re
from contextlib import suppress
from enum import Enum

import attr

from mozilla_version.errors import MissingFieldError, PatternNotMatchedError
from mozilla_version.parser import (
    does_regex_have_group,
    get_value_matched_by_regex,
    positive_int,
    positive_int_or_none,
)


@attr.s(frozen=True, eq=False, hash=True)
class BaseVersion:
    """Class that validates and handles general version numbers."""

    major_number = attr.ib(type=int, converter=positive_int)
    minor_number = attr.ib(type=int, converter=positive_int)
    patch_number = attr.ib(type=int, converter=positive_int_or_none, default=None)

    _MANDATORY_NUMBERS = ("major_number", "minor_number")
    _OPTIONAL_NUMBERS = ("patch_number",)
    _ALL_NUMBERS = _MANDATORY_NUMBERS + _OPTIONAL_NUMBERS

    _VALID_ENOUGH_VERSION_PATTERN = re.compile(
        r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?$""",
        re.VERBOSE,
    )

    @classmethod
    def parse(cls, version_string, regex_groups=()):
        """Construct an object representing a valid version number."""
        regex_matches = cls._VALID_ENOUGH_VERSION_PATTERN.match(version_string)

        if regex_matches is None:
            raise PatternNotMatchedError(
                version_string, (cls._VALID_ENOUGH_VERSION_PATTERN,)
            )

        kwargs = {}

        for field in cls._MANDATORY_NUMBERS:
            kwargs[field] = get_value_matched_by_regex(
                field, regex_matches, version_string
            )
        for field in cls._OPTIONAL_NUMBERS:
            with suppress(MissingFieldError):
                kwargs[field] = get_value_matched_by_regex(
                    field, regex_matches, version_string
                )

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

        return ".".join(semvers)

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
            raise ValueError(f'Cannot compare "{other}", type not supported!')

        for field in ("major_number", "minor_number", "patch_number"):
            difference = self._substract_other_number_from_this_number(other, field)
            if difference != 0:
                return difference

        return 0

    def _substract_other_number_from_this_number(self, other, field):
        # BaseVersion sets unmatched numbers to None. E.g.: "32.0" sets the patch_number
        # to None. Because of this behavior, `getattr(self, 'patch_number')` returns
        # None too. That's why we can't call `getattr(self, field, 0)` directly, it will
        # return None for all unmatched numbers
        this_number = getattr(self, field, None)
        this_number = 0 if this_number is None else this_number
        other_number = getattr(other, field, None)
        other_number = 0 if other_number is None else other_number

        return this_number - other_number

    def bump(self, field):
        """Bump the number defined `field`.

        Returns:
            A new BaseVersion with the right field bumped and the following ones set to
            0, if they exist or if they need to be set.

            For instance:
             * 32.0 is bumped to 33.0, because the patch number does not exist
             * 32.0.1 is bumped to 33.0.0, because the patch number exists
             * 32.0 is bumped to 32.1.0, because patch number must be defined if the
               minor number is not 0.

        """
        try:
            return self.__class__(**self._create_bump_kwargs(field))
        except (ValueError, PatternNotMatchedError) as e:
            raise ValueError(
                f'Cannot bump "{field}". New version number is not valid. Cause: {e}'
            ) from e

    def _create_bump_kwargs(self, field):
        if field not in self._ALL_NUMBERS:
            raise ValueError(f'Unknown field "{field}"')

        kwargs = {}
        has_requested_field_been_met = False
        should_set_optional_numbers = False
        for current_field in self._ALL_NUMBERS:
            current_number = getattr(self, current_field, None)
            if current_field == field:
                has_requested_field_been_met = True
                new_number = 1 if current_number is None else current_number + 1
                if new_number == 1 and current_field == "minor_number":
                    should_set_optional_numbers = True
                kwargs[current_field] = new_number
            else:
                if has_requested_field_been_met and (
                    current_field not in self._OPTIONAL_NUMBERS
                    or should_set_optional_numbers
                    or current_number is not None
                ):
                    new_number = 0
                else:
                    new_number = current_number
                kwargs[current_field] = new_number

        return kwargs


class VersionType(Enum):
    """Enum that sorts types of versions (e.g.: nightly, beta, release, esr).

    Supports comparison. `ESR` is considered higher than `RELEASE` (even if they
    technically have the same codebase). For instance: 60.0.1 < 60.0.1esr but
    61.0 > 60.0.1esr. This choice has a practical use case: if you have a list of
    Release and ESR version, you can easily extract one kind or the other thanks to the
    VersionType.

    Examples:
        .. code-block:: python

            assert VersionType.NIGHTLY == VersionType.NIGHTLY
            assert VersionType.ESR > VersionType.RELEASE

    """

    NIGHTLY = 1
    AURORA_OR_DEVEDITION = 2
    BETA = 3
    RELEASE_CANDIDATE = 4
    RELEASE = 5
    ESR = 6

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


@attr.s(frozen=True, eq=False, hash=True)
class ShipItVersion(BaseVersion):
    """Class that represent a version in the way ShipIt intends it to be.

    ShipIt is Release Engineering's https://github.com/mozilla-releng/shipit/
    """

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        error_messages = self._get_all_error_messages_for_attributes()
        if error_messages:
            raise PatternNotMatchedError(self, patterns=error_messages)

    def _get_all_error_messages_for_attributes(self):
        return [
            pattern_message
            for condition, pattern_message in (
                (
                    self.is_major and self.is_stability,
                    "Version cannot be both a major and a stability one",
                ),
                (
                    self.is_major and self.is_development,
                    "Version cannot be both a major and a development one",
                ),
                (
                    self.is_stability and self.is_development,
                    "Version cannot be both a stability and a development one",
                ),
            )
            if condition
        ]

    @property
    def is_major(self):
        """Return `True` if `ShipItVersion` is considered to be a major version.

        It's usually the .0 release but some exceptions may occur.
        """
        return all(
            (not self.is_development, self.minor_number == 0, self.patch_number is None)
        )

    @property
    def is_stability(self):
        """Return `True` if `ShipItVersion` is a version that fixed a major one."""
        return all(
            (
                not self.is_development,
                not self.is_major,
                self.minor_number != 0 or self.patch_number != 0,
            )
        )

    @property
    def is_development(self):
        """Return `True` if `ShipItVersion` was known to require further development.

        It's usually a beta or before the rapid release scheme, a release candidate.
        """
        # TODO Bubble up beta_number and release_candidate number in ShipItVersion
        return self.is_beta or self.is_release_candidate
