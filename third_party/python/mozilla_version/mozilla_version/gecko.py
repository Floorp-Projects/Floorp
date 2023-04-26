"""Defines characteristics of a Gecko version number, including Firefox.

Examples:
    .. code-block:: python

        from mozilla_version.gecko import FirefoxVersion

        version = FirefoxVersion.parse('60.0.1')

        version.major_number    # 60
        version.minor_number    # 0
        version.patch_number    # 1

        version.is_release  # True
        version.is_beta     # False
        version.is_nightly  # False

        str(version)        # '60.0.1'

        previous_version = FirefoxVersion.parse('60.0b14')
        previous_version < version      # True

        previous_version.beta_number    # 14
        previous_version.major_number   # 60
        previous_version.minor_number   # 0
        previous_version.patch_number   # raises AttributeError

        previous_version.is_beta     # True
        previous_version.is_release  # False
        previous_version.is_nightly  # False

        invalid_version = FirefoxVersion.parse('60.1')      # raises PatternNotMatchedError
        invalid_version = FirefoxVersion.parse('60.0.0')    # raises PatternNotMatchedError
        version = FirefoxVersion.parse('60.0')    # valid

        # Versions can be built by raw values
        FirefoxVersion(60, 0))         # '60.0'
        FirefoxVersion(60, 0, 1))      # '60.0.1'
        FirefoxVersion(60, 1, 0))      # '60.1.0'
        FirefoxVersion(60, 0, 1, 1))   # '60.0.1build1'
        FirefoxVersion(60, 0, beta_number=1))       # '60.0b1'
        FirefoxVersion(60, 0, is_nightly=True))     # '60.0a1'
        FirefoxVersion(60, 0, is_aurora_or_devedition=True))    # '60.0a2'
        FirefoxVersion(60, 0, is_esr=True))         # '60.0esr'
        FirefoxVersion(60, 0, 1, is_esr=True))      # '60.0.1esr'

"""

import attr
import re

from mozilla_version.errors import (
    PatternNotMatchedError, TooManyTypesError, NoVersionTypeError
)
from mozilla_version.parser import strictly_positive_int_or_none
from mozilla_version.version import BaseVersion, VersionType


def _find_type(version):
    version_type = None

    def ensure_version_type_is_not_already_defined(previous_type, candidate_type):
        if previous_type is not None:
            raise TooManyTypesError(
                str(version), previous_type, candidate_type
            )

    if version.is_nightly:
        version_type = VersionType.NIGHTLY
    if version.is_aurora_or_devedition:
        ensure_version_type_is_not_already_defined(
            version_type, VersionType.AURORA_OR_DEVEDITION
        )
        version_type = VersionType.AURORA_OR_DEVEDITION
    if version.is_beta:
        ensure_version_type_is_not_already_defined(version_type, VersionType.BETA)
        version_type = VersionType.BETA
    if version.is_esr:
        ensure_version_type_is_not_already_defined(version_type, VersionType.ESR)
        version_type = VersionType.ESR
    if version.is_release_candidate:
        ensure_version_type_is_not_already_defined(version_type, VersionType.RELEASE_CANDIDATE)
        version_type = VersionType.RELEASE_CANDIDATE
    if version.is_release:
        ensure_version_type_is_not_already_defined(version_type, VersionType.RELEASE)
        version_type = VersionType.RELEASE

    if version_type is None:
        raise NoVersionTypeError(str(version))

    return version_type


@attr.s(frozen=True, eq=False, hash=True)
class GeckoVersion(BaseVersion):
    """Class that validates and handles version numbers for Gecko-based products.

    You may want to use specific classes like FirefoxVersion. These classes define edge cases
    that were shipped.

    Raises:
        PatternNotMatchedError: if the string doesn't match the pattern of a valid version number
        MissingFieldError: if a mandatory field is missing in the string. Mandatory fields are
            `major_number` and `minor_number`
        ValueError: if an integer can't be cast or is not (strictly) positive
        TooManyTypesError: if the string matches more than 1 `VersionType`
        NoVersionTypeError: if the string matches none.

    """

    # XXX This pattern doesn't catch all subtleties of a Firefox version (like 32.5 isn't valid).
    # This regex is intended to assign numbers. Then checks are done by attrs and
    # __attrs_post_init__()
    _VALID_ENOUGH_VERSION_PATTERN = re.compile(r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?
        (\.(?P<old_fourth_number>\d+))?
        (
            (?P<is_nightly>a1)
            |(?P<is_aurora_or_devedition>a2)
            |rc(?P<release_candidate_number>\d+)
            |b(?P<beta_number>\d+)
            |(?P<is_esr>esr)
        )?
        -?(build(?P<build_number>\d+))?$""", re.VERBOSE)

    _OPTIONAL_NUMBERS = BaseVersion._OPTIONAL_NUMBERS + (
        'old_fourth_number', 'release_candidate_number', 'beta_number', 'build_number'
    )

    _ALL_NUMBERS = BaseVersion._ALL_NUMBERS + _OPTIONAL_NUMBERS

    _KNOWN_ESR_MAJOR_NUMBERS = (10, 17, 24, 31, 38, 45, 52, 60, 68, 78, 91, 102, 115)

    _LAST_AURORA_DEVEDITION_AS_VERSION_TYPE = 54

    build_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    beta_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    is_nightly = attr.ib(type=bool, default=False)
    is_aurora_or_devedition = attr.ib(type=bool, default=False)
    is_esr = attr.ib(type=bool, default=False)
    old_fourth_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    release_candidate_number = attr.ib(
        type=int, converter=strictly_positive_int_or_none, default=None
    )
    version_type = attr.ib(init=False, default=attr.Factory(_find_type, takes_self=True))

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        # General checks
        error_messages = [
            pattern_message
            for condition, pattern_message in ((
                not self.is_four_digit_scheme and self.old_fourth_number is not None,
                'The old fourth number can only be defined on Gecko 1.5.x.y or 2.0.x.y',
            ), (
                self.beta_number is not None and self.patch_number is not None,
                'Beta number and patch number cannot be both defined',
            ))
            if condition
        ]

        # Firefox 5 is the first version to implement the rapid release model, which defines
        # the scheme used so far.
        if self.is_rapid_release_scheme:
            error_messages.extend([
                pattern_message
                for condition, pattern_message in ((
                    self.release_candidate_number is not None,
                    'Release candidate number cannot be defined starting Gecko 5',
                ), (
                    self.minor_number == 0 and self.patch_number == 0,
                    'Minor number and patch number cannot be both equal to 0',
                ), (
                    self.minor_number != 0 and self.patch_number is None,
                    'Patch number cannot be undefined if minor number is greater than 0',
                ), (
                    self.patch_number is not None and self.is_nightly,
                    'Patch number cannot be defined on a nightly version',
                ), (
                    self.patch_number is not None and self.is_aurora_or_devedition,
                    'Patch number cannot be defined on an aurora version',
                ), (
                    self.major_number > self._LAST_AURORA_DEVEDITION_AS_VERSION_TYPE and
                    self.is_aurora_or_devedition,
                    'Last aurora/devedition version was 54.0a2. Please use the DeveditionVersion '
                    'class, past this version.',
                ), (
                    self.major_number not in self._KNOWN_ESR_MAJOR_NUMBERS and self.is_esr,
                    '"{}" is not a valid ESR major number. Valid ones are: {}'.format(
                        self.major_number, self._KNOWN_ESR_MAJOR_NUMBERS
                    )
                ))
                if condition
            ])
        else:
            if self.release_candidate_number is not None:
                error_messages.extend([
                    pattern_message
                    for condition, pattern_message in ((
                        self.patch_number is not None,
                        'Release candidate and patch number cannot be both defined',
                    ), (
                        self.old_fourth_number is not None,
                        'Release candidate and the old fourth number cannot be both defined',
                    ), (
                        self.beta_number is not None,
                        'Release candidate and beta number cannot be both defined',
                    ))
                    if condition
                ])

            if self.old_fourth_number is not None and self.patch_number != 0:
                error_messages.append(
                    'The old fourth number cannot be defined if the patch number is not 0 '
                    '(we have never shipped a release that did so)'
                )

        if error_messages:
            raise PatternNotMatchedError(self, patterns=error_messages)

    @classmethod
    def parse(cls, version_string):
        """Construct an object representing a valid Firefox version number."""
        return super().parse(
            version_string, regex_groups=('is_nightly', 'is_aurora_or_devedition', 'is_esr')
        )

    @property
    def is_beta(self):
        """Return `True` if `GeckoVersion` was built with a string matching a beta version."""
        return self.beta_number is not None

    @property
    def is_release_candidate(self):
        """Return `True` if `GeckoVersion` was built with a string matching an RC version."""
        return self.release_candidate_number is not None

    @property
    def is_rapid_release_scheme(self):
        """Return `True` if `GeckoVersion` was built with against the rapid release scheme."""
        return self.major_number >= 5

    @property
    def is_four_digit_scheme(self):
        """Return `True` if `GeckoVersion` was built with the 4 digits schemes.

        Only Firefox 1.5.x.y and 2.0.x.y were.
        """
        return (
            all((self.major_number == 1, self.minor_number == 5)) or
            all((self.major_number == 2, self.minor_number == 0))
        )

    @property
    def is_release(self):
        """Return `True` if `GeckoVersion` was built with a string matching a release version."""
        return not any((
            self.is_nightly, self.is_aurora_or_devedition, self.is_beta,
            self.is_release_candidate, self.is_esr
        ))

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        string = super().__str__()

        if self.old_fourth_number is not None:
            string = f'{string}.{self.old_fourth_number}'

        if self.is_nightly:
            string = f'{string}a1'
        elif self.is_aurora_or_devedition:
            string = f'{string}a2'
        elif self.is_beta:
            string = f'{string}b{self.beta_number}'
        elif self.is_release_candidate:
            string = f'{string}rc{self.release_candidate_number}'
        elif self.is_esr:
            string = f'{string}esr'

        if self.build_number is not None:
            string = f'{string}build{self.build_number}'

        return string

    def __eq__(self, other):
        """Implement `==` operator.

        A version is considered equal to another if all numbers match and if they are of the same
        `VersionType`. Like said in `VersionType`, release and ESR are considered equal (if they
        share the same numbers). If a version contains a build number but not the other, the build
        number won't be considered in the comparison.

        Examples:
            .. code-block:: python

                assert GeckoVersion.parse('60.0') == GeckoVersion.parse('60.0')
                assert GeckoVersion.parse('60.0') == GeckoVersion.parse('60.0esr')
                assert GeckoVersion.parse('60.0') == GeckoVersion.parse('60.0build1')
                assert GeckoVersion.parse('60.0build1') == GeckoVersion.parse('60.0build1')

                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('61.0')
                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('60.1.0')
                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('60.0.1')
                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('60.0a1')
                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('60.0a2')
                assert GeckoVersion.parse('60.0') != GeckoVersion.parse('60.0b1')
                assert GeckoVersion.parse('60.0build1') != GeckoVersion.parse('60.0build2')

        """
        return super().__eq__(other)

    def _compare(self, other):
        """Compare this release with another.

        Returns:
            0 if equal
            < 0 is this precedes the other
            > 0 if the other precedes this

        """
        if isinstance(other, str):
            other = GeckoVersion.parse(other)
        elif not isinstance(other, GeckoVersion):
            raise ValueError(f'Cannot compare "{other}", type not supported!')

        difference = super()._compare(other)
        if difference != 0:
            return difference

        difference = self._substract_other_number_from_this_number(other, 'old_fourth_number')
        if difference != 0:
            return difference

        channel_difference = self._compare_version_type(other)
        if channel_difference != 0:
            return channel_difference

        if self.is_beta and other.is_beta:
            beta_difference = self.beta_number - other.beta_number
            if beta_difference != 0:
                return beta_difference

        if self.is_release_candidate and other.is_release_candidate:
            rc_difference = self.release_candidate_number - other.release_candidate_number
            if rc_difference != 0:
                return rc_difference

        # Build numbers are a special case. We might compare a regular version number
        # (like "32.0b8") versus a release build (as in "32.0b8build1"). As a consequence,
        # we only compare build_numbers when we both have them.
        try:
            return self.build_number - other.build_number
        except TypeError:
            pass

        return 0

    def _compare_version_type(self, other):
        return self.version_type.compare(other.version_type)

    def _create_bump_kwargs(self, field):
        if field == 'build_number' and self.build_number is None:
            raise ValueError('Cannot bump the build number if it is not already set')

        bump_kwargs = super()._create_bump_kwargs(field)

        if field == 'major_number' and self.is_esr:
            current_esr_index = self._KNOWN_ESR_MAJOR_NUMBERS.index(self.major_number)
            try:
                next_major_esr_number = self._KNOWN_ESR_MAJOR_NUMBERS[current_esr_index + 1]
            except IndexError:
                raise ValueError(
                    "Cannot bump the major number past last known major ESR. We don't know it yet."
                )
            bump_kwargs['major_number'] = next_major_esr_number

        if field != 'build_number' and bump_kwargs.get('build_number') == 0:
            del bump_kwargs['build_number']
        if bump_kwargs.get('beta_number') == 0:
            if self.is_beta:
                bump_kwargs['beta_number'] = 1
            else:
                del bump_kwargs['beta_number']

        if field != 'old_fourth_number' and not self.is_four_digit_scheme:
            del bump_kwargs['old_fourth_number']
            if bump_kwargs.get('minor_number') == 0 and bump_kwargs.get('patch_number') == 0:
                del bump_kwargs['patch_number']

        if self.is_four_digit_scheme:
            if (
                bump_kwargs.get('patch_number') == 0 and
                bump_kwargs.get('old_fourth_number') in (0, None)
            ):
                del bump_kwargs['patch_number']
                del bump_kwargs['old_fourth_number']
            elif (
                bump_kwargs.get('patch_number') is None and
                bump_kwargs.get('old_fourth_number') is not None and
                bump_kwargs.get('old_fourth_number') > 0
            ):
                bump_kwargs['patch_number'] = 0

        if field != 'release_candidate_number' and self.is_rapid_release_scheme:
            del bump_kwargs['release_candidate_number']

        bump_kwargs['is_nightly'] = self.is_nightly
        bump_kwargs['is_aurora_or_devedition'] = self.is_aurora_or_devedition
        bump_kwargs['is_esr'] = self.is_esr

        return bump_kwargs

    def bump_version_type(self):
        """Bump version type to the next one.

        Returns:
            A new GeckoVersion with the version type set to the next one. Builds numbers are reset,
            if originally set.

            For instance:
             * 32.0a1 is bumped to 32.0b1
             * 32.0bX is bumped to 32.0
             * 32.0 is bumped to 32.0esr
             * 31.0build1 is bumped to 31.0esrbuild1
             * 31.0build2 is bumped to 31.0esrbuild1

        """
        try:
            return self.__class__(**self._create_bump_version_type_kwargs())
        except (ValueError, PatternNotMatchedError) as e:
            raise ValueError(
                'Cannot bump version type for version "{}". New version number is not valid. '
                'Cause: {}'.format(self, e)
            ) from e

    def _create_bump_version_type_kwargs(self):
        bump_version_type_kwargs = {
            'major_number': self.major_number,
            'minor_number': self.minor_number,
            'patch_number': self.patch_number,
        }

        if self.is_nightly and self.major_number <= self._LAST_AURORA_DEVEDITION_AS_VERSION_TYPE:
            bump_version_type_kwargs['is_aurora_or_devedition'] = True
        elif (
            self.is_nightly and self.major_number > self._LAST_AURORA_DEVEDITION_AS_VERSION_TYPE or
            self.is_aurora_or_devedition
        ):
            bump_version_type_kwargs['beta_number'] = 1
        elif self.is_beta and not self.is_rapid_release_scheme:
            bump_version_type_kwargs['release_candidate_number'] = 1
        elif self.is_release:
            bump_version_type_kwargs['is_esr'] = True
        elif self.is_esr:
            raise ValueError('There is no higher version type than ESR.')

        if self.build_number is not None:
            bump_version_type_kwargs['build_number'] = 1

        return bump_version_type_kwargs


class _VersionWithEdgeCases(GeckoVersion):
    def __attrs_post_init__(self):
        for edge_case in self._RELEASED_EDGE_CASES:
            if all(
                getattr(self, number_type) == edge_case.get(number_type, None)
                for number_type in self._ALL_NUMBERS
                if number_type != 'build_number'
            ):
                if self.build_number is None:
                    return
                elif self.build_number == edge_case.get('build_number', None):
                    return

        super().__attrs_post_init__()


class FirefoxVersion(_VersionWithEdgeCases):
    """Class that validates and handles Firefox version numbers."""

    _RELEASED_EDGE_CASES = ({
        'major_number': 1,
        'minor_number': 5,
        'patch_number': 0,
        'old_fourth_number': 1,
        'release_candidate_number': 1,
    }, {
        'major_number': 33,
        'minor_number': 1,
        'build_number': 1,
    }, {
        'major_number': 33,
        'minor_number': 1,
        'build_number': 2,
    }, {
        'major_number': 33,
        'minor_number': 1,
        'build_number': 3,
    }, {
        'major_number': 38,
        'minor_number': 0,
        'patch_number': 5,
        'beta_number': 1,
        'build_number': 1,
    }, {
        'major_number': 38,
        'minor_number': 0,
        'patch_number': 5,
        'beta_number': 1,
        'build_number': 2,
    }, {
        'major_number': 38,
        'minor_number': 0,
        'patch_number': 5,
        'beta_number': 2,
        'build_number': 1,
    }, {
        'major_number': 38,
        'minor_number': 0,
        'patch_number': 5,
        'beta_number': 3,
        'build_number': 1,
    })


class DeveditionVersion(GeckoVersion):
    """Class that validates and handles Devedition after it became an equivalent to beta."""

    # No edge case were shipped

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        if (
            (not self.is_beta) or
            (self.major_number < 54) or
            (self.major_number == 54 and self.beta_number < 11)
        ):
            raise PatternNotMatchedError(
                self, patterns=('Devedition as a product must be a beta >= 54.0b11',)
            )


class FennecVersion(_VersionWithEdgeCases):
    """Class that validates and handles Fennec (Firefox for Android) version numbers."""

    _RELEASED_EDGE_CASES = ({
        'major_number': 33,
        'minor_number': 1,
        'build_number': 1,
    }, {
        'major_number': 33,
        'minor_number': 1,
        'build_number': 2,
    }, {
        'major_number': 38,
        'minor_number': 0,
        'patch_number': 5,
        'beta_number': 4,
        'build_number': 1,
    })

    _LAST_FENNEC_VERSION = 68

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        # Versions matching 68.Xa1, 68.XbN, or simply 68.X are expected since bug 1523402. The
        # latter is needed because of the version.txt of beta
        if (
            self.major_number == self._LAST_FENNEC_VERSION and
            self.minor_number > 0 and
            self.patch_number is None
        ):
            return

        if self.major_number > self._LAST_FENNEC_VERSION:
            raise PatternNotMatchedError(
                self, patterns=(f'Last Fennec version is {self._LAST_FENNEC_VERSION}',)
            )

        super().__attrs_post_init__()

    def _create_bump_kwargs(self, field):
        kwargs = super()._create_bump_kwargs(field)

        if (
            field != 'patch_number' and
            kwargs['major_number'] == self._LAST_FENNEC_VERSION and
            (kwargs['is_nightly'] or kwargs.get('beta_number'))
        ):
            del kwargs['patch_number']

        return kwargs


class ThunderbirdVersion(_VersionWithEdgeCases):
    """Class that validates and handles Thunderbird version numbers."""

    _RELEASED_EDGE_CASES = ({
        'major_number': 1,
        'minor_number': 5,
        'beta_number': 1,
    }, {
        'major_number': 1,
        'minor_number': 5,
        'beta_number': 2,
    }, {
        'major_number': 3,
        'minor_number': 1,
        'beta_number': 1,
    }, {
        'major_number': 3,
        'minor_number': 1,
    }, {
        'major_number': 45,
        'minor_number': 1,
        'beta_number': 1,
        'build_number': 1,
    }, {
        'major_number': 45,
        'minor_number': 2,
        'build_number': 1,
    }, {
        'major_number': 45,
        'minor_number': 2,
        'build_number': 2,
    }, {
        'major_number': 45,
        'minor_number': 2,
        'beta_number': 1,
        'build_number': 2,
    })


class GeckoSnapVersion(GeckoVersion):
    """Class that validates and handles Gecko's Snap version numbers.

    Snap is a Linux packaging format developped by Canonical. Valid numbers are like "63.0b7-1",
    "1" stands for "build1". Release Engineering set this scheme at the beginning of Snap and now
    we can't rename published snap to the regular pattern like "63.0b7-build1".
    """

    # Our Snaps are recent enough to not list any edge case, yet.

    # Differences between this regex and the one in GeckoVersion:
    #   * no a2
    #   * no "build"
    #   * but mandatory dash and build number.
    # Example: 63.0b7-1
    _VALID_ENOUGH_VERSION_PATTERN = re.compile(r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?
        (
            (?P<is_nightly>a1)
            |b(?P<beta_number>\d+)
            |(?P<is_esr>esr)
        )?
        -(?P<build_number>\d+)$""", re.VERBOSE)

    def __str__(self):
        """Implement string representation.

        Returns format like "63.0b7-1"
        """
        string = super().__str__()
        return string.replace('build', '-')
