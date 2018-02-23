from __future__ import absolute_import

from distutils.version import StrictVersion
import re


class ModernMozillaVersion(StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
       Instead of just allowing "a" or "b" as prerelease tags, it allows any
       alpha. This allows us to support the once-shipped "3.6.3plugin1" and
       similar versions."""
    version_re = re.compile(r"""^(\d+) \. (\d+) (\. (\d+))?
                                ([a-zA-Z]+(\d+))?$""", re.VERBOSE)


class AncientMozillaVersion(StrictVersion):
    """A version class that is slightly less restrictive than StrictVersion.
       Instead of just allowing "a" or "b" as prerelease tags, it allows any
       alpha. This allows us to support the once-shipped "3.6.3plugin1" and
       similar versions.
       It also supports versions w.x.y.z by transmuting to w.x.z, which
       is useful for versions like 1.5.0.x and 2.0.0.y"""
    version_re = re.compile(r"""^(\d+) \. (\d+) \. \d (\. (\d+))
                                ([a-zA-Z]+(\d+))?$""", re.VERBOSE)


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
    raise ValueError("Version number %s is invalid." % version)


def getPrettyVersion(version):
    version = re.sub(r'a([0-9]+)$', r' Alpha \1', version)
    version = re.sub(r'b([0-9]+)$', r' Beta \1', version)
    version = re.sub(r'rc([0-9]+)$', r' RC \1', version)
    return version
