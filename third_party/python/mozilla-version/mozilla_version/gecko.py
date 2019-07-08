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
    if version.is_release:
        ensure_version_type_is_not_already_defined(version_type, VersionType.RELEASE)
        version_type = VersionType.RELEASE

    if version_type is None:
        raise NoVersionTypeError(str(version))

    return version_type


@attr.s(frozen=True, cmp=False, hash=True)
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
        (
            (?P<is_nightly>a1)
            |(?P<is_aurora_or_devedition>a2)
            |b(?P<beta_number>\d+)
            |(?P<is_esr>esr)
        )?
        -?(build(?P<build_number>\d+))?$""", re.VERBOSE)

    _ALL_VERSION_NUMBERS_TYPES = (
        'major_number', 'minor_number', 'patch_number', 'beta_number',
    )

    _OPTIONAL_NUMBERS = BaseVersion._OPTIONAL_NUMBERS + ('beta_number', 'build_number')

    build_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    beta_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    is_nightly = attr.ib(type=bool, default=False)
    is_aurora_or_devedition = attr.ib(type=bool, default=False)
    is_esr = attr.ib(type=bool, default=False)
    version_type = attr.ib(init=False, default=attr.Factory(_find_type, takes_self=True))

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        if self.minor_number == 0 and self.patch_number == 0:
            raise PatternNotMatchedError(
                self, pattern='Minor number and patch number cannot be both equal to 0'
            )

        if self.minor_number != 0 and self.patch_number is None:
            raise PatternNotMatchedError(
                self, pattern='Patch number cannot be undefined if minor number is greater than 0'
            )

        if self.beta_number is not None and self.patch_number is not None:
            raise PatternNotMatchedError(
                self, pattern='Beta number and patch number cannot be both defined'
            )

        if self.patch_number is not None and self.is_nightly:
            raise PatternNotMatchedError(
                self, pattern='Patch number cannot be defined on a nightly version'
            )

        if self.patch_number is not None and self.is_aurora_or_devedition:
            raise PatternNotMatchedError(
                self, pattern='Patch number cannot be defined on an aurora version'
            )

    @classmethod
    def parse(cls, version_string):
        """Construct an object representing a valid Firefox version number."""
        return super(GeckoVersion, cls).parse(
            version_string, regex_groups=('is_nightly', 'is_aurora_or_devedition', 'is_esr')
        )

    @property
    def is_beta(self):
        """Return `True` if `FirefoxVersion` was built with a string matching a beta version."""
        return self.beta_number is not None

    @property
    def is_release(self):
        """Return `True` if `FirefoxVersion` was built with a string matching a release version."""
        return not (self.is_nightly or self.is_aurora_or_devedition or self.is_beta or self.is_esr)

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        string = super(GeckoVersion, self).__str__()

        if self.is_nightly:
            string = '{}a1'.format(string)
        elif self.is_aurora_or_devedition:
            string = '{}a2'.format(string)
        elif self.is_beta:
            string = '{}b{}'.format(string, self.beta_number)
        elif self.is_esr:
            string = '{}esr'.format(string)

        if self.build_number is not None:
            string = '{}build{}'.format(string, self.build_number)

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
        return super(GeckoVersion, self).__eq__(other)

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
            raise ValueError('Cannot compare "{}", type not supported!'.format(other))

        difference = super(GeckoVersion, self)._compare(other)
        if difference != 0:
            return difference

        channel_difference = self._compare_version_type(other)
        if channel_difference != 0:
            return channel_difference

        if self.is_beta and other.is_beta:
            beta_difference = self.beta_number - other.beta_number
            if beta_difference != 0:
                return beta_difference

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


class _VersionWithEdgeCases(GeckoVersion):
    def __attrs_post_init__(self):
        for edge_case in self._RELEASED_EDGE_CASES:
            if all(
                getattr(self, number_type) == edge_case.get(number_type, None)
                for number_type in self._ALL_VERSION_NUMBERS_TYPES
            ):
                if self.build_number is None:
                    return
                elif self.build_number == edge_case.get('build_number', None):
                    return

        super(_VersionWithEdgeCases, self).__attrs_post_init__()


class FirefoxVersion(_VersionWithEdgeCases):
    """Class that validates and handles Firefox version numbers."""

    _RELEASED_EDGE_CASES = ({
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
                self, pattern='Devedition as a product must be a beta >= 54.0b11'
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

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        # Versions matching 68.Xa1, 68.XbN, or simply 68.X are expected since bug 1523402. The
        # latter is needed because of the version.txt of beta
        if (
            self.major_number == 68 and
            self.minor_number > 0 and
            self.patch_number is None
        ):
            return

        if self.major_number >= 69:
            raise PatternNotMatchedError(self, pattern='Last Fennec version is 68')

        super(FennecVersion, self).__attrs_post_init__()


class ThunderbirdVersion(_VersionWithEdgeCases):
    """Class that validates and handles Thunderbird version numbers."""

    _RELEASED_EDGE_CASES = ({
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
        string = super(GeckoSnapVersion, self).__str__()
        return string.replace('build', '-')
