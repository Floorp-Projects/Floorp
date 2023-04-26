"""Defines characteristics of a Mobile version at Mozilla."""

import attr
import re

from mozilla_version.errors import PatternNotMatchedError, TooManyTypesError, NoVersionTypeError
from mozilla_version.gecko import GeckoVersion
from mozilla_version.version import BaseVersion, VersionType
from mozilla_version.parser import strictly_positive_int_or_none


def _find_type(version):
    version_type = None

    def ensure_version_type_is_not_already_defined(previous_type, candidate_type):
        if previous_type is not None:
            raise TooManyTypesError(
                str(version), previous_type, candidate_type
            )

    if version.is_nightly:
        version_type = VersionType.NIGHTLY
    if version.is_beta:
        ensure_version_type_is_not_already_defined(version_type, VersionType.BETA)
        version_type = VersionType.BETA
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
class MobileVersion(BaseVersion):
    """Validate and handle version numbers for mobile products.

    This covers applications such as Fenix and Focus for Android.
    """

    _VALID_ENOUGH_VERSION_PATTERN = re.compile(r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?
        (
            (?P<is_nightly>a1)
            |(-beta\.|b)(?P<beta_number>\d+)
            |-rc\.(?P<release_candidate_number>\d+)
        )?
        -?(build(?P<build_number>\d+))?$""", re.VERBOSE)

    _OPTIONAL_NUMBERS = (
        'patch_number', 'beta_number', 'release_candidate_number', 'build_number'
    )

    _ALL_NUMBERS = BaseVersion._MANDATORY_NUMBERS + _OPTIONAL_NUMBERS

    # Focus-Android and Fenix were the first ones to be converted to the Gecko
    # pattern (bug 1777255)
    _FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN = 104
    # Android-Components later (bug 1800611)
    _LAST_VERSION_TO_FOLLOW_MAVEN_PATTERN = 108

    build_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    beta_number = attr.ib(type=int, converter=strictly_positive_int_or_none, default=None)
    is_nightly = attr.ib(type=bool, default=False)
    release_candidate_number = attr.ib(
        type=int, converter=strictly_positive_int_or_none, default=None
    )
    version_type = attr.ib(init=False, default=attr.Factory(_find_type, takes_self=True))

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        error_messages = []

        if self.is_gecko_pattern:
            error_messages.extend([
                pattern_message
                for condition, pattern_message in ((
                    self.beta_number is not None and self.patch_number is not None,
                    'Beta number and patch number cannot be both defined',
                ), (
                    self.release_candidate_number is not None,
                    'Release candidate number cannot be defined after Mobile v{}'.format(
                        self._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
                    ),
                ), (
                    self.major_number > self._LAST_VERSION_TO_FOLLOW_MAVEN_PATTERN and
                    self.minor_number == 0 and
                    self.patch_number == 0,
                    'Minor number and patch number cannot be both equal to 0 past '
                    'Mobile v{}'.format(
                        self._LAST_VERSION_TO_FOLLOW_MAVEN_PATTERN
                    ),
                ), (
                    self.minor_number != 0 and self.patch_number is None,
                    'Patch number cannot be undefined if minor number is greater than 0',
                ))
                if condition
            ])
        else:
            error_messages.extend([
                pattern_message
                for condition, pattern_message in ((
                    self.patch_number is None,
                    'Patch number must be defined before Mobile v{}'.format(
                        self._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
                    ),
                ), (
                    self.is_nightly,
                    'Nightlies are not supported until Mobile v{}'.format(
                        self._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
                    ),
                ))
                if condition
            ])

        if error_messages:
            raise PatternNotMatchedError(self, patterns=error_messages)

    @classmethod
    def parse(cls, version_string):
        """Construct an object representing a valid Firefox version number."""
        mobile_version = super().parse(
            version_string, regex_groups=('is_nightly',)
        )

        # Betas are supported in both the old and the gecko pattern. Let's make sure
        # the string we got follows the right rules
        if mobile_version.is_beta:
            if mobile_version.is_gecko_pattern and '-beta.' in version_string:
                raise PatternNotMatchedError(
                    mobile_version, ['"-beta." can only be used before Mobile v{}'.format(
                        cls._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
                    )]
                )
            if not mobile_version.is_gecko_pattern and re.search(r"\db\d", version_string):
                raise PatternNotMatchedError(
                    mobile_version, [
                        '"b" cannot be used before Mobile v{} to define a '
                        'beta version'.format(
                            cls._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
                        )
                    ]
                )

        return mobile_version

    @property
    def is_gecko_pattern(self):
        """Return `True` if `MobileVersion` was built with against the Gecko scheme."""
        return self.major_number >= self._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN

    @property
    def is_beta(self):
        """Return `True` if `MobileVersion` was built with a string matching a beta version."""
        return self.beta_number is not None

    @property
    def is_release_candidate(self):
        """Return `True` if `MobileVersion` was built with a string matching an RC version."""
        return self.release_candidate_number is not None

    @property
    def is_release(self):
        """Return `True` if `MobileVersion` was built with a string matching a release version."""
        return not any((
            self.is_nightly, self.is_beta, self.is_release_candidate,
        ))

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        if self.is_gecko_pattern:
            string = str(GeckoVersion(
                major_number=self.major_number,
                minor_number=self.minor_number,
                patch_number=self.patch_number,
                build_number=self.build_number,
                beta_number=self.beta_number,
                is_nightly=self.is_nightly,
            ))
        else:
            string = super().__str__()
            if self.is_beta:
                string = f'{string}-beta.{self.beta_number}'
            elif self.is_release_candidate:
                string = f'{string}-rc.{self.release_candidate_number}'

        return string

    def _compare(self, other):
        if isinstance(other, str):
            other = MobileVersion.parse(other)
        elif not isinstance(other, MobileVersion):
            raise ValueError(f'Cannot compare "{other}", type not supported!')

        difference = super()._compare(other)
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

        return 0

    def _compare_version_type(self, other):
        return self.version_type.compare(other.version_type)

    def _create_bump_kwargs(self, field):
        bump_kwargs = super()._create_bump_kwargs(field)

        if field != 'build_number' and bump_kwargs.get('build_number') == 0:
            del bump_kwargs['build_number']
        if bump_kwargs.get('beta_number') == 0:
            if self.is_beta:
                bump_kwargs['beta_number'] = 1
            else:
                del bump_kwargs['beta_number']

        if field != 'release_candidate_number':
            del bump_kwargs['release_candidate_number']

        if (
            field == 'major_number'
            and bump_kwargs.get('major_number') == self._FIRST_VERSION_TO_FOLLOW_GECKO_PATTERN
        ):
            del bump_kwargs['patch_number']

        bump_kwargs['is_nightly'] = self.is_nightly

        return bump_kwargs
