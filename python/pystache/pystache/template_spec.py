# coding: utf-8

"""
Provides a class to customize template information on a per-view basis.

To customize template properties for a particular view, create that view
from a class that subclasses TemplateSpec.  The "spec" in TemplateSpec
stands for "special" or "specified" template information.

"""

class TemplateSpec(object):

    """
    A mixin or interface for specifying custom template information.

    The "spec" in TemplateSpec can be taken to mean that the template
    information is either "specified" or "special."

    A view should subclass this class only if customized template loading
    is needed.  The following attributes allow one to customize/override
    template information on a per view basis.  A None value means to use
    default behavior for that value and perform no customization.  All
    attributes are initialized to None.

    Attributes:

      template: the template as a string.

      template_encoding: the encoding used by the template.

      template_extension: the template file extension.  Defaults to "mustache".
        Pass False for no extension (i.e. extensionless template files).

      template_name: the name of the template.

      template_path: absolute path to the template.

      template_rel_directory: the directory containing the template file,
        relative to the directory containing the module defining the class.

      template_rel_path: the path to the template file, relative to the
        directory containing the module defining the class.

    """

    template = None
    template_encoding = None
    template_extension = None
    template_name = None
    template_path = None
    template_rel_directory = None
    template_rel_path = None
