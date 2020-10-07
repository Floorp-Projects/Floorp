from __future__ import absolute_import

from ._compat import ABC, FileNotFoundError
from abc import abstractmethod

# We use mypy's comment syntax here since this file must be compatible with
# both Python 2 and 3.
try:
    from typing import BinaryIO, Iterable, Text                   # noqa: F401
except ImportError:
    # Python 2
    pass


class ResourceReader(ABC):
    """Abstract base class for loaders to provide resource reading support."""

    @abstractmethod
    def open_resource(self, resource):
        # type: (Text) -> BinaryIO
        """Return an opened, file-like object for binary reading.

        The 'resource' argument is expected to represent only a file name.
        If the resource cannot be found, FileNotFoundError is raised.
        """
        # This deliberately raises FileNotFoundError instead of
        # NotImplementedError so that if this method is accidentally called,
        # it'll still do the right thing.
        raise FileNotFoundError

    @abstractmethod
    def resource_path(self, resource):
        # type: (Text) -> Text
        """Return the file system path to the specified resource.

        The 'resource' argument is expected to represent only a file name.
        If the resource does not exist on the file system, raise
        FileNotFoundError.
        """
        # This deliberately raises FileNotFoundError instead of
        # NotImplementedError so that if this method is accidentally called,
        # it'll still do the right thing.
        raise FileNotFoundError

    @abstractmethod
    def is_resource(self, path):
        # type: (Text) -> bool
        """Return True if the named 'path' is a resource.

        Files are resources, directories are not.
        """
        raise FileNotFoundError

    @abstractmethod
    def contents(self):
        # type: () -> Iterable[str]
        """Return an iterable of entries in `package`."""
        raise FileNotFoundError
