# coding: utf-8

"""
This module supports customized (aka special or specified) template loading.

"""

import os.path

from pystache.loader import Loader


# TODO: add test cases for this class.
class SpecLoader(object):

    """
    Supports loading custom-specified templates (from TemplateSpec instances).

    """

    def __init__(self, loader=None):
        if loader is None:
            loader = Loader()

        self.loader = loader

    def _find_relative(self, spec):
        """
        Return the path to the template as a relative (dir, file_name) pair.

        The directory returned is relative to the directory containing the
        class definition of the given object.  The method returns None for
        this directory if the directory is unknown without first searching
        the search directories.

        """
        if spec.template_rel_path is not None:
            return os.path.split(spec.template_rel_path)
        # Otherwise, determine the file name separately.

        locator = self.loader._make_locator()

        # We do not use the ternary operator for Python 2.4 support.
        if spec.template_name is not None:
            template_name = spec.template_name
        else:
            template_name = locator.make_template_name(spec)

        file_name = locator.make_file_name(template_name, spec.template_extension)

        return (spec.template_rel_directory, file_name)

    def _find(self, spec):
        """
        Find and return the path to the template associated to the instance.

        """
        if spec.template_path is not None:
            return spec.template_path

        dir_path, file_name = self._find_relative(spec)

        locator = self.loader._make_locator()

        if dir_path is None:
            # Then we need to search for the path.
            path = locator.find_object(spec, self.loader.search_dirs, file_name=file_name)
        else:
            obj_dir = locator.get_object_directory(spec)
            path = os.path.join(obj_dir, dir_path, file_name)

        return path

    def load(self, spec):
        """
        Find and return the template associated to a TemplateSpec instance.

        Returns the template as a unicode string.

        Arguments:

          spec: a TemplateSpec instance.

        """
        if spec.template is not None:
            return self.loader.unicode(spec.template, spec.template_encoding)

        path = self._find(spec)

        return self.loader.read(path, spec.template_encoding)
