# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from distutils.version import StrictVersion, LooseVersion
import re
from six import PY2


def _cmp(cls, first, other):
    # Because the __cmp__ metamethod was removed in the switch to Python 3, the
    # interface of the distutils.version.*Version classes changed in a
    # backwards-incompatible way. Call this instead of first.__cmp__(other) or
    # first._cmp(other) to handle both possibilities.
    if PY2:
        return cls.__cmp__(first, other)
    return cls._cmp(first, other)


class MozillaVersionCompareMixin():
    def __cmp__(self, other):
        # We expect this function to never be called.
        raise AssertionError()

    def _cmp(self, other):
        has_esr = set()
        if isinstance(other, LooseModernMozillaVersion) and str(other).endswith('esr'):
            # If other version ends with esr, coerce through MozillaVersion ending up with
            # a StrictVersion if possible
            has_esr.add('other')
            other = MozillaVersion(str(other)[:-3])  # strip ESR from end of string
        if isinstance(self, LooseModernMozillaVersion) and str(self).endswith('esr'):
            # If our version ends with esr, coerce through MozillaVersion ending up with
            # a StrictVersion if possible
            has_esr.add('self')
            self = MozillaVersion(str(self)[:-3])  # strip ESR from end of string
        if isinstance(other, LooseModernMozillaVersion) or \
                isinstance(self, LooseModernMozillaVersion):
            # If we're still LooseVersion for self or other, run LooseVersion compare
            # Being sure to pass through Loose Version type first
            val = _cmp(LooseVersion,
                       LooseModernMozillaVersion(str(self)),
                       LooseModernMozillaVersion(str(other)))
        else:
            # No versions are loose, therefore we can use StrictVersion
            val = _cmp(StrictVersion, self, other)
        if has_esr.isdisjoint(set(['other', 'self'])) or \
                has_esr.issuperset(set(['other', 'self'])):
            #  If both had esr string or neither, then _cmp() was accurate
            return val
        elif val != 0:
            # cmp is accurate here even if esr is present in only 1 compare, since
            # versions are not equal
            return val
        elif 'other' in has_esr:
            return -1  # esr is not greater than non esr
        return 1  # non esr is greater than esr

    # These method definitions can be deleted when we drop support for Python 2.
    def __eq__(self, other):
        return self._cmp(other) == 0

    def __lt__(self, other):
        return self._cmp(other) < 0

    def __le__(self, other):
        return self._cmp(other) <= 0

    def __gt__(self, other):
        return self._cmp(other) > 0

    def __ge__(self, other):
        return self._cmp(other) >= 0


class ModernMozillaVersion(MozillaVersionCompareMixin, StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
       Instead of just allowing "a" or "b" as prerelease tags, it allows any
       alpha. This allows us to support the once-shipped "3.6.3plugin1" and
       similar versions."""
    version_re = re.compile(r"""^(\d+) \. (\d+) (\. (\d+))?
                                ([a-zA-Z]+(\d+))?$""", re.VERBOSE)


class AncientMozillaVersion(MozillaVersionCompareMixin, StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
       Instead of just allowing "a" or "b" as prerelease tags, it allows any
       alpha. This allows us to support the once-shipped "3.6.3plugin1" and
       similar versions.
       It also supports versions w.x.y.z by transmuting to w.x.z, which
       is useful for versions like 1.5.0.x and 2.0.0.y"""
    version_re = re.compile(r"""^(\d+) \. (\d+) \. \d (\. (\d+))
                                ([a-zA-Z]+(\d+))?$""", re.VERBOSE)


class LooseModernMozillaVersion(MozillaVersionCompareMixin, LooseVersion):
    """A version class that is more restrictive than LooseVersion.
       This class reduces the valid strings to "esr", "a", "b" and "rc" in order
       to support esr. StrictVersion requires a trailing number after all strings."""
    component_re = re.compile(r'(\d+ | a | b | rc | esr | \.)', re.VERBOSE)

    def __repr__(self):
        return "LooseModernMozillaVersion ('%s')" % str(self)


def MozillaVersion(version):
    try:
        return ModernMozillaVersion(version)
    except ValueError:
        pass
    try:
        if version.count('.') == 3:
            return AncientMozillaVersion(version)
    except ValueError:
        pass
    try:
        return LooseModernMozillaVersion(version)
    except ValueError:
        pass
    raise ValueError("Version number %s is invalid." % version)


def getPrettyVersion(version):
    version = re.sub(r'a([0-9]+)$', r' Alpha \1', version)
    version = re.sub(r'b([0-9]+)$', r' Beta \1', version)
    version = re.sub(r'rc([0-9]+)$', r' RC \1', version)
    return version
