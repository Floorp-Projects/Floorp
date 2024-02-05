# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from looseversion import LooseVersion
from packaging.version import InvalidVersion


class StrictVersion:
    def __init__(self, vstring):
        self.parse(vstring)

    def __repr__(self):
        return "%s ('%s')" % (self.__class__.__name__, str(self))

    def __eq__(self, other):
        return self._cmp(other) == 0

    def __lt__(self, other):
        return self._cmp(other) < 0

    def parse(self, vstring):
        match = self.version_re.match(vstring)
        if not match:
            raise InvalidVersion("invalid version number '%s'" % vstring)

        major, minor, patch, pre, pre_num = match.group(1, 2, 4, 5, 6)
        self.version = int(major), int(minor), int(patch or 0)
        self.pre = (pre[0], int(pre_num)) if pre else ()

    def __str__(self):
        return ".".join(map(str, self.version)) + (
            "".join(map(str, self.pre)) if self.pre else ""
        )

    def _cmp(self, other):
        if isinstance(other, str):
            other = StrictVersion(other)
        elif not isinstance(other, StrictVersion):
            raise NotImplementedError

        if self.version < other.version:
            return -1
        elif self.version == other.version:
            if self.pre == other.pre:
                return 0
            elif not self.pre:
                return 1
            elif not other.pre:
                return -1
            elif self.pre < other.pre:
                return -1
            else:
                return 1
        else:
            return 1


class MozillaVersionCompareMixin:
    def __cmp__(self, other):
        # We expect this function to never be called.
        raise AssertionError()

    def _cmp(self, other):
        has_esr = set()
        if isinstance(other, LooseModernMozillaVersion) and str(other).endswith("esr"):
            # If other version ends with esr, coerce through MozillaVersion ending up with
            # a StrictVersion if possible
            has_esr.add("other")
            other = MozillaVersion(str(other)[:-3])  # strip ESR from end of string
        if isinstance(self, LooseModernMozillaVersion) and str(self).endswith("esr"):
            # If our version ends with esr, coerce through MozillaVersion ending up with
            # a StrictVersion if possible
            has_esr.add("self")
            self = MozillaVersion(str(self)[:-3])  # strip ESR from end of string
        if isinstance(other, LooseModernMozillaVersion) or isinstance(
            self, LooseModernMozillaVersion
        ):
            # If we're still LooseVersion for self or other, run LooseVersion compare
            # Being sure to pass through Loose Version type first
            val = LooseVersion._cmp(
                LooseModernMozillaVersion(str(self)),
                LooseModernMozillaVersion(str(other)),
            )
        else:
            # No versions are loose, therefore we can use StrictVersion
            val = StrictVersion._cmp(self, other)
        if has_esr.isdisjoint(set(["other", "self"])) or has_esr.issuperset(
            set(["other", "self"])
        ):
            #  If both had esr string or neither, then _cmp() was accurate
            return val
        elif val != 0:
            # cmp is accurate here even if esr is present in only 1 compare, since
            # versions are not equal
            return val
        elif "other" in has_esr:
            return -1  # esr is not greater than non esr
        return 1  # non esr is greater than esr


class ModernMozillaVersion(MozillaVersionCompareMixin, StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
    Instead of just allowing "a" or "b" as prerelease tags, it allows any
    alpha. This allows us to support the once-shipped "3.6.3plugin1" and
    similar versions."""

    version_re = re.compile(
        r"""^(\d+) \. (\d+) (\. (\d+))?
                                ([a-zA-Z]+(\d+))?$""",
        re.VERBOSE,
    )


class AncientMozillaVersion(MozillaVersionCompareMixin, StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
    Instead of just allowing "a" or "b" as prerelease tags, it allows any
    alpha. This allows us to support the once-shipped "3.6.3plugin1" and
    similar versions.
    It also supports versions w.x.y.z by transmuting to w.x.z, which
    is useful for versions like 1.5.0.x and 2.0.0.y"""

    version_re = re.compile(
        r"""^(\d+) \. (\d+) \. \d (\. (\d+))
                                ([a-zA-Z]+(\d+))?$""",
        re.VERBOSE,
    )


class LooseModernMozillaVersion(MozillaVersionCompareMixin, LooseVersion):
    """A version class that is more restrictive than LooseVersion.
    This class reduces the valid strings to "esr", "a", "b" and "rc" in order
    to support esr. StrictVersion requires a trailing number after all strings."""

    component_re = re.compile(r"(\d+ | a | b | rc | esr | \.)", re.VERBOSE)

    def __repr__(self):
        return "LooseModernMozillaVersion ('%s')" % str(self)


def MozillaVersion(version):
    try:
        return ModernMozillaVersion(version)
    except InvalidVersion:
        pass
    try:
        if version.count(".") == 3:
            return AncientMozillaVersion(version)
    except InvalidVersion:
        pass
    try:
        return LooseModernMozillaVersion(version)
    except ValueError:
        pass
    raise ValueError("Version number %s is invalid." % version)


def getPrettyVersion(version):
    version = re.sub(r"a([0-9]+)$", r" Alpha \1", version)
    version = re.sub(r"b([0-9]+)$", r" Beta \1", version)
    version = re.sub(r"rc([0-9]+)$", r" RC \1", version)
    return version
