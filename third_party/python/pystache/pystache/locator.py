# coding: utf-8

"""
This module provides a Locator class for finding template files.

"""

import os
import re
import sys

from pystache.common import TemplateNotFoundError
from pystache import defaults


class Locator(object):

    def __init__(self, extension=None):
        """
        Construct a template locator.

        Arguments:

          extension: the template file extension, without the leading dot.
            Pass False for no extension (e.g. to use extensionless template
            files).  Defaults to the package default.

        """
        if extension is None:
            extension = defaults.TEMPLATE_EXTENSION

        self.template_extension = extension

    def get_object_directory(self, obj):
        """
        Return the directory containing an object's defining class.

        Returns None if there is no such directory, for example if the
        class was defined in an interactive Python session, or in a
        doctest that appears in a text file (rather than a Python file).

        """
        if not hasattr(obj, '__module__'):
            return None

        module = sys.modules[obj.__module__]

        if not hasattr(module, '__file__'):
            # TODO: add a unit test for this case.
            return None

        path = module.__file__

        return os.path.dirname(path)

    def make_template_name(self, obj):
        """
        Return the canonical template name for an object instance.

        This method converts Python-style class names (PEP 8's recommended
        CamelCase, aka CapWords) to lower_case_with_underscords.  Here
        is an example with code:

        >>> class HelloWorld(object):
        ...     pass
        >>> hi = HelloWorld()
        >>>
        >>> locator = Locator()
        >>> locator.make_template_name(hi)
        'hello_world'

        """
        template_name = obj.__class__.__name__

        def repl(match):
            return '_' + match.group(0).lower()

        return re.sub('[A-Z]', repl, template_name)[1:]

    def make_file_name(self, template_name, template_extension=None):
        """
        Generate and return the file name for the given template name.

        Arguments:

          template_extension: defaults to the instance's extension.

        """
        file_name = template_name

        if template_extension is None:
            template_extension = self.template_extension

        if template_extension is not False:
            file_name += os.path.extsep + template_extension

        return file_name

    def _find_path(self, search_dirs, file_name):
        """
        Search for the given file, and return the path.

        Returns None if the file is not found.

        """
        for dir_path in search_dirs:
            file_path = os.path.join(dir_path, file_name)
            if os.path.exists(file_path):
                return file_path

        return None

    def _find_path_required(self, search_dirs, file_name):
        """
        Return the path to a template with the given file name.

        """
        path = self._find_path(search_dirs, file_name)

        if path is None:
            raise TemplateNotFoundError('File %s not found in dirs: %s' %
                                        (repr(file_name), repr(search_dirs)))

        return path

    def find_file(self, file_name, search_dirs):
        """
        Return the path to a template with the given file name.

        Arguments:

          file_name: the file name of the template.

          search_dirs: the list of directories in which to search.

        """
        return self._find_path_required(search_dirs, file_name)

    def find_name(self, template_name, search_dirs):
        """
        Return the path to a template with the given name.

        Arguments:

          template_name: the name of the template.

          search_dirs: the list of directories in which to search.

        """
        file_name = self.make_file_name(template_name)

        return self._find_path_required(search_dirs, file_name)

    def find_object(self, obj, search_dirs, file_name=None):
        """
        Return the path to a template associated with the given object.

        """
        if file_name is None:
            # TODO: should we define a make_file_name() method?
            template_name = self.make_template_name(obj)
            file_name = self.make_file_name(template_name)

        dir_path = self.get_object_directory(obj)

        if dir_path is not None:
            search_dirs = [dir_path] + search_dirs

        path = self._find_path_required(search_dirs, file_name)

        return path
