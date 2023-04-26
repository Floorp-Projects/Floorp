"""Defines characteristics of a Maven version at Mozilla."""

import attr
import re

from mozilla_version.version import BaseVersion


@attr.s(frozen=True, eq=False, hash=True)
class MavenVersion(BaseVersion):
    """Class that validates and handles Maven version numbers.

    At Mozilla, Maven packages are used in projects like "GeckoView" or "Android-Components".
    """

    is_snapshot = attr.ib(type=bool, default=False)
    is_beta = attr.ib(type=bool, default=False, init=False)
    is_release_candidate = attr.ib(type=bool, default=False, init=False)

    _VALID_ENOUGH_VERSION_PATTERN = re.compile(r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (\.(?P<patch_number>\d+))?
        (?P<is_snapshot>-SNAPSHOT)?$""", re.VERBOSE)

    @classmethod
    def parse(cls, version_string):
        """Construct an object representing a valid Maven version number."""
        return super().parse(version_string, regex_groups=('is_snapshot', ))

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        string = super().__str__()

        if self.is_snapshot:
            string = f'{string}-SNAPSHOT'

        return string

    def _compare(self, other):
        if isinstance(other, str):
            other = MavenVersion.parse(other)
        elif not isinstance(other, MavenVersion):
            raise ValueError(f'Cannot compare "{other}", type not supported!')

        difference = super()._compare(other)
        if difference != 0:
            return difference

        if not self.is_snapshot and other.is_snapshot:
            return 1
        elif self.is_snapshot and not other.is_snapshot:
            return -1
        else:
            return 0

    @property
    def is_release(self):
        """Return `True` if the others are both False."""
        return not any((
            self.is_beta, self.is_release_candidate, self.is_snapshot
        ))
