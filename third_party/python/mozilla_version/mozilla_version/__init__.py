"""Defines characteristics of Mozilla's version numbers."""

from mozilla_version.gecko import (
    DeveditionVersion,
    FirefoxVersion,
    ThunderbirdVersion,
)
from mozilla_version.maven import MavenVersion
from mozilla_version.mobile import MobileVersion

__all__ = [
    "DeveditionVersion",
    "FirefoxVersion",
    "MavenVersion",
    "MobileVersion",
    "ThunderbirdVersion",
]
