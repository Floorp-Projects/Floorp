# coding: utf-8

"""
This module provides a Loader class for locating and reading templates.

"""

import os
import sys

from pystache import common
from pystache import defaults
from pystache.locator import Locator


# We make a function so that the current defaults take effect.
# TODO: revisit whether this is necessary.

def _make_to_unicode():
    def to_unicode(s, encoding=None):
        """
        Raises a TypeError exception if the given string is already unicode.

        """
        if encoding is None:
            encoding = defaults.STRING_ENCODING
        return unicode(s, encoding, defaults.DECODE_ERRORS)
    return to_unicode


class Loader(object):

    """
    Loads the template associated to a name or user-defined object.

    All load_*() methods return the template as a unicode string.

    """

    def __init__(self, file_encoding=None, extension=None, to_unicode=None,
                 search_dirs=None):
        """
        Construct a template loader instance.

        Arguments:

          extension: the template file extension, without the leading dot.
            Pass False for no extension (e.g. to use extensionless template
            files).  Defaults to the package default.

          file_encoding: the name of the encoding to use when converting file
            contents to unicode.  Defaults to the package default.

          search_dirs: the list of directories in which to search when loading
            a template by name or file name.  Defaults to the package default.

          to_unicode: the function to use when converting strings of type
            str to unicode.  The function should have the signature:

              to_unicode(s, encoding=None)

            It should accept a string of type str and an optional encoding
            name and return a string of type unicode.  Defaults to calling
            Python's built-in function unicode() using the package string
            encoding and decode errors defaults.

        """
        if extension is None:
            extension = defaults.TEMPLATE_EXTENSION

        if file_encoding is None:
            file_encoding = defaults.FILE_ENCODING

        if search_dirs is None:
            search_dirs = defaults.SEARCH_DIRS

        if to_unicode is None:
            to_unicode = _make_to_unicode()

        self.extension = extension
        self.file_encoding = file_encoding
        # TODO: unit test setting this attribute.
        self.search_dirs = search_dirs
        self.to_unicode = to_unicode

    def _make_locator(self):
        return Locator(extension=self.extension)

    def unicode(self, s, encoding=None):
        """
        Convert a string to unicode using the given encoding, and return it.

        This function uses the underlying to_unicode attribute.

        Arguments:

          s: a basestring instance to convert to unicode.  Unlike Python's
            built-in unicode() function, it is okay to pass unicode strings
            to this function.  (Passing a unicode string to Python's unicode()
            with the encoding argument throws the error, "TypeError: decoding
            Unicode is not supported.")

          encoding: the encoding to pass to the to_unicode attribute.
            Defaults to None.

        """
        if isinstance(s, unicode):
            return unicode(s)

        return self.to_unicode(s, encoding)

    def read(self, path, encoding=None):
        """
        Read the template at the given path, and return it as a unicode string.

        """
        b = common.read(path)

        if encoding is None:
            encoding = self.file_encoding

        return self.unicode(b, encoding)

    def load_file(self, file_name):
        """
        Find and return the template with the given file name.

        Arguments:

          file_name: the file name of the template.

        """
        locator = self._make_locator()

        path = locator.find_file(file_name, self.search_dirs)

        return self.read(path)

    def load_name(self, name):
        """
        Find and return the template with the given template name.

        Arguments:

          name: the name of the template.

        """
        locator = self._make_locator()

        path = locator.find_name(name, self.search_dirs)

        return self.read(path)

    # TODO: unit-test this method.
    def load_object(self, obj):
        """
        Find and return the template associated to the given object.

        Arguments:

          obj: an instance of a user-defined class.

          search_dirs: the list of directories in which to search.

        """
        locator = self._make_locator()

        path = locator.find_object(obj, self.search_dirs)

        return self.read(path)
