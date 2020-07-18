"""Defines characteristics of a Balrog release name.

Balrog is the server that delivers Firefox and Thunderbird updates. Release names follow
the pattern "{product}-{version}-build{build_number}"

Examples:
    .. code-block:: python

        from mozilla_version.balrog import BalrogReleaseName

        balrog_release = BalrogReleaseName.parse('firefox-60.0.1-build1')

        balrog_release.product                 # firefox
        balrog_release.version.major_number    # 60
        str(balrog_release)                    # 'firefox-60.0.1-build1'

        previous_release = BalrogReleaseName.parse('firefox-60.0-build2')
        previous_release < balrog_release      # True

        invalid = BalrogReleaseName.parse('60.0.1')           # raises PatternNotMatchedError
        invalid = BalrogReleaseName.parse('firefox-60.0.1')   # raises PatternNotMatchedError

        # Releases can be built thanks to version classes like FirefoxVersion
        BalrogReleaseName('firefox', FirefoxVersion(60, 0, 1, 1))  # 'firefox-60.0.1-build1'

"""

import attr
import re

from mozilla_version.errors import PatternNotMatchedError
from mozilla_version.parser import get_value_matched_by_regex
from mozilla_version.gecko import (
    GeckoVersion, FirefoxVersion, DeveditionVersion, FennecVersion, ThunderbirdVersion
)


_VALID_ENOUGH_BALROG_RELEASE_PATTERN = re.compile(
    r"^(?P<product>[a-z]+)-(?P<version>.+)$", re.IGNORECASE
)


_SUPPORTED_PRODUCTS = {
    'firefox': FirefoxVersion,
    'devedition': DeveditionVersion,
    'fennec': FennecVersion,
    'thunderbird': ThunderbirdVersion,
}


def _supported_product(string):
    product = string.lower()
    if product not in _SUPPORTED_PRODUCTS:
        raise PatternNotMatchedError(string, pattern='unknown product')
    return product


def _products_must_be_identical(method):
    def checker(this, other):
        if this.product != other.product:
            raise ValueError('Cannot compare "{}" and "{}"'.format(this.product, other.product))
        return method(this, other)
    return checker


@attr.s(frozen=True, cmp=False, hash=True)
class BalrogReleaseName(object):
    """Class that validates and handles Balrog release names.

    Raises:
        PatternNotMatchedError: if a parsed string doesn't match the pattern of a valid release
        MissingFieldError: if a mandatory field is missing in the string. Mandatory fields are
            `product`, `major_number`, `minor_number`, and `build_number`
        ValueError: if an integer can't be cast or is not (strictly) positive
        TooManyTypesError: if the string matches more than 1 `VersionType`
        NoVersionTypeError: if the string matches none.

    """

    product = attr.ib(type=str, converter=_supported_product)
    version = attr.ib(type=GeckoVersion)

    def __attrs_post_init__(self):
        """Ensure attributes are sane all together."""
        if self.version.build_number is None:
            raise PatternNotMatchedError(self, pattern='build_number must exist')

    @classmethod
    def parse(cls, release_string):
        """Construct an object representing a valid Firefox version number."""
        regex_matches = _VALID_ENOUGH_BALROG_RELEASE_PATTERN.match(release_string)
        if regex_matches is None:
            raise PatternNotMatchedError(release_string, _VALID_ENOUGH_BALROG_RELEASE_PATTERN)

        product = get_value_matched_by_regex('product', regex_matches, release_string)
        try:
            VersionClass = _SUPPORTED_PRODUCTS[product.lower()]
        except KeyError:
            raise PatternNotMatchedError(release_string, pattern='unknown product')

        version_string = get_value_matched_by_regex('version', regex_matches, release_string)
        version = VersionClass.parse(version_string)

        return cls(product, version)

    def __str__(self):
        """Implement string representation.

        Computes a new string based on the given attributes.
        """
        version_string = str(self.version).replace('build', '-build')
        return '{}-{}'.format(self.product, version_string)

    @_products_must_be_identical
    def __eq__(self, other):
        """Implement `==` operator."""
        return self.version == other.version

    @_products_must_be_identical
    def __ne__(self, other):
        """Implement `!=` operator."""
        return self.version != other.version

    @_products_must_be_identical
    def __lt__(self, other):
        """Implement `<` operator."""
        return self.version < other.version

    @_products_must_be_identical
    def __le__(self, other):
        """Implement `<=` operator."""
        return self.version <= other.version

    @_products_must_be_identical
    def __gt__(self, other):
        """Implement `>` operator."""
        return self.version > other.version

    @_products_must_be_identical
    def __ge__(self, other):
        """Implement `>=` operator."""
        return self.version >= other.version
