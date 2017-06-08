# coding: utf-8

"""
This module provides a Renderer class to render templates.

"""

import sys

from pystache import defaults
from pystache.common import TemplateNotFoundError, MissingTags, is_string
from pystache.context import ContextStack, KeyNotFoundError
from pystache.loader import Loader
from pystache.parsed import ParsedTemplate
from pystache.renderengine import context_get, RenderEngine
from pystache.specloader import SpecLoader
from pystache.template_spec import TemplateSpec


class Renderer(object):

    """
    A class for rendering mustache templates.

    This class supports several rendering options which are described in
    the constructor's docstring.  Other behavior can be customized by
    subclassing this class.

    For example, one can pass a string-string dictionary to the constructor
    to bypass loading partials from the file system:

    >>> partials = {'partial': 'Hello, {{thing}}!'}
    >>> renderer = Renderer(partials=partials)
    >>> # We apply print to make the test work in Python 3 after 2to3.
    >>> print renderer.render('{{>partial}}', {'thing': 'world'})
    Hello, world!

    To customize string coercion (e.g. to render False values as ''), one can
    subclass this class.  For example:

        class MyRenderer(Renderer):
            def str_coerce(self, val):
                if not val:
                    return ''
                else:
                    return str(val)

    """

    def __init__(self, file_encoding=None, string_encoding=None,
                 decode_errors=None, search_dirs=None, file_extension=None,
                 escape=None, partials=None, missing_tags=None):
        """
        Construct an instance.

        Arguments:

          file_encoding: the name of the encoding to use by default when
            reading template files.  All templates are converted to unicode
            prior to parsing.  Defaults to the package default.

          string_encoding: the name of the encoding to use when converting
            to unicode any byte strings (type str in Python 2) encountered
            during the rendering process.  This name will be passed as the
            encoding argument to the built-in function unicode().
            Defaults to the package default.

          decode_errors: the string to pass as the errors argument to the
            built-in function unicode() when converting byte strings to
            unicode.  Defaults to the package default.

          search_dirs: the list of directories in which to search when
            loading a template by name or file name.  If given a string,
            the method interprets the string as a single directory.
            Defaults to the package default.

          file_extension: the template file extension.  Pass False for no
            extension (i.e. to use extensionless template files).
            Defaults to the package default.

          partials: an object (e.g. a dictionary) for custom partial loading
            during the rendering process.
                The object should have a get() method that accepts a string
            and returns the corresponding template as a string, preferably
            as a unicode string.  If there is no template with that name,
            the get() method should either return None (as dict.get() does)
            or raise an exception.
                If this argument is None, the rendering process will use
            the normal procedure of locating and reading templates from
            the file system -- using relevant instance attributes like
            search_dirs, file_encoding, etc.

          escape: the function used to escape variable tag values when
            rendering a template.  The function should accept a unicode
            string (or subclass of unicode) and return an escaped string
            that is again unicode (or a subclass of unicode).
                This function need not handle strings of type `str` because
            this class will only pass it unicode strings.  The constructor
            assigns this function to the constructed instance's escape()
            method.
                To disable escaping entirely, one can pass `lambda u: u`
            as the escape function, for example.  One may also wish to
            consider using markupsafe's escape function: markupsafe.escape().
            This argument defaults to the package default.

          missing_tags: a string specifying how to handle missing tags.
            If 'strict', an error is raised on a missing tag.  If 'ignore',
            the value of the tag is the empty string.  Defaults to the
            package default.

        """
        if decode_errors is None:
            decode_errors = defaults.DECODE_ERRORS

        if escape is None:
            escape = defaults.TAG_ESCAPE

        if file_encoding is None:
            file_encoding = defaults.FILE_ENCODING

        if file_extension is None:
            file_extension = defaults.TEMPLATE_EXTENSION

        if missing_tags is None:
            missing_tags = defaults.MISSING_TAGS

        if search_dirs is None:
            search_dirs = defaults.SEARCH_DIRS

        if string_encoding is None:
            string_encoding = defaults.STRING_ENCODING

        if isinstance(search_dirs, basestring):
            search_dirs = [search_dirs]

        self._context = None
        self.decode_errors = decode_errors
        self.escape = escape
        self.file_encoding = file_encoding
        self.file_extension = file_extension
        self.missing_tags = missing_tags
        self.partials = partials
        self.search_dirs = search_dirs
        self.string_encoding = string_encoding

    # This is an experimental way of giving views access to the current context.
    # TODO: consider another approach of not giving access via a property,
    #   but instead letting the caller pass the initial context to the
    #   main render() method by reference.  This approach would probably
    #   be less likely to be misused.
    @property
    def context(self):
        """
        Return the current rendering context [experimental].

        """
        return self._context

    # We could not choose str() as the name because 2to3 renames the unicode()
    # method of this class to str().
    def str_coerce(self, val):
        """
        Coerce a non-string value to a string.

        This method is called whenever a non-string is encountered during the
        rendering process when a string is needed (e.g. if a context value
        for string interpolation is not a string).  To customize string
        coercion, you can override this method.

        """
        return str(val)

    def _to_unicode_soft(self, s):
        """
        Convert a basestring to unicode, preserving any unicode subclass.

        """
        # We type-check to avoid "TypeError: decoding Unicode is not supported".
        # We avoid the Python ternary operator for Python 2.4 support.
        if isinstance(s, unicode):
            return s
        return self.unicode(s)

    def _to_unicode_hard(self, s):
        """
        Convert a basestring to a string with type unicode (not subclass).

        """
        return unicode(self._to_unicode_soft(s))

    def _escape_to_unicode(self, s):
        """
        Convert a basestring to unicode (preserving any unicode subclass), and escape it.

        Returns a unicode string (not subclass).

        """
        return unicode(self.escape(self._to_unicode_soft(s)))

    def unicode(self, b, encoding=None):
        """
        Convert a byte string to unicode, using string_encoding and decode_errors.

        Arguments:

          b: a byte string.

          encoding: the name of an encoding.  Defaults to the string_encoding
            attribute for this instance.

        Raises:

          TypeError: Because this method calls Python's built-in unicode()
            function, this method raises the following exception if the
            given string is already unicode:

              TypeError: decoding Unicode is not supported

        """
        if encoding is None:
            encoding = self.string_encoding

        # TODO: Wrap UnicodeDecodeErrors with a message about setting
        # the string_encoding and decode_errors attributes.
        return unicode(b, encoding, self.decode_errors)

    def _make_loader(self):
        """
        Create a Loader instance using current attributes.

        """
        return Loader(file_encoding=self.file_encoding, extension=self.file_extension,
                      to_unicode=self.unicode, search_dirs=self.search_dirs)

    def _make_load_template(self):
        """
        Return a function that loads a template by name.

        """
        loader = self._make_loader()

        def load_template(template_name):
            return loader.load_name(template_name)

        return load_template

    def _make_load_partial(self):
        """
        Return a function that loads a partial by name.

        """
        if self.partials is None:
            return self._make_load_template()

        # Otherwise, create a function from the custom partial loader.
        partials = self.partials

        def load_partial(name):
            # TODO: consider using EAFP here instead.
            #     http://docs.python.org/glossary.html#term-eafp
            #   This would mean requiring that the custom partial loader
            #   raise a KeyError on name not found.
            template = partials.get(name)
            if template is None:
                raise TemplateNotFoundError("Name %s not found in partials: %s" %
                                            (repr(name), type(partials)))

            # RenderEngine requires that the return value be unicode.
            return self._to_unicode_hard(template)

        return load_partial

    def _is_missing_tags_strict(self):
        """
        Return whether missing_tags is set to strict.

        """
        val = self.missing_tags

        if val == MissingTags.strict:
            return True
        elif val == MissingTags.ignore:
            return False

        raise Exception("Unsupported 'missing_tags' value: %s" % repr(val))

    def _make_resolve_partial(self):
        """
        Return the resolve_partial function to pass to RenderEngine.__init__().

        """
        load_partial = self._make_load_partial()

        if self._is_missing_tags_strict():
            return load_partial
        # Otherwise, ignore missing tags.

        def resolve_partial(name):
            try:
                return load_partial(name)
            except TemplateNotFoundError:
                return u''

        return resolve_partial

    def _make_resolve_context(self):
        """
        Return the resolve_context function to pass to RenderEngine.__init__().

        """
        if self._is_missing_tags_strict():
            return context_get
        # Otherwise, ignore missing tags.

        def resolve_context(stack, name):
            try:
                return context_get(stack, name)
            except KeyNotFoundError:
                return u''

        return resolve_context

    def _make_render_engine(self):
        """
        Return a RenderEngine instance for rendering.

        """
        resolve_context = self._make_resolve_context()
        resolve_partial = self._make_resolve_partial()

        engine = RenderEngine(literal=self._to_unicode_hard,
                              escape=self._escape_to_unicode,
                              resolve_context=resolve_context,
                              resolve_partial=resolve_partial,
                              to_str=self.str_coerce)
        return engine

    # TODO: add unit tests for this method.
    def load_template(self, template_name):
        """
        Load a template by name from the file system.

        """
        load_template = self._make_load_template()
        return load_template(template_name)

    def _render_object(self, obj, *context, **kwargs):
        """
        Render the template associated with the given object.

        """
        loader = self._make_loader()

        # TODO: consider an approach that does not require using an if
        #   block here.  For example, perhaps this class's loader can be
        #   a SpecLoader in all cases, and the SpecLoader instance can
        #   check the object's type.  Or perhaps Loader and SpecLoader
        #   can be refactored to implement the same interface.
        if isinstance(obj, TemplateSpec):
            loader = SpecLoader(loader)
            template = loader.load(obj)
        else:
            template = loader.load_object(obj)

        context = [obj] + list(context)

        return self._render_string(template, *context, **kwargs)

    def render_name(self, template_name, *context, **kwargs):
        """
        Render the template with the given name using the given context.

        See the render() docstring for more information.

        """
        loader = self._make_loader()
        template = loader.load_name(template_name)
        return self._render_string(template, *context, **kwargs)

    def render_path(self, template_path, *context, **kwargs):
        """
        Render the template at the given path using the given context.

        Read the render() docstring for more information.

        """
        loader = self._make_loader()
        template = loader.read(template_path)

        return self._render_string(template, *context, **kwargs)

    def _render_string(self, template, *context, **kwargs):
        """
        Render the given template string using the given context.

        """
        # RenderEngine.render() requires that the template string be unicode.
        template = self._to_unicode_hard(template)

        render_func = lambda engine, stack: engine.render(template, stack)

        return self._render_final(render_func, *context, **kwargs)

    # All calls to render() should end here because it prepares the
    # context stack correctly.
    def _render_final(self, render_func, *context, **kwargs):
        """
        Arguments:

          render_func: a function that accepts a RenderEngine and ContextStack
            instance and returns a template rendering as a unicode string.

        """
        stack = ContextStack.create(*context, **kwargs)
        self._context = stack

        engine = self._make_render_engine()

        return render_func(engine, stack)

    def render(self, template, *context, **kwargs):
        """
        Render the given template string, view template, or parsed template.

        Returns a unicode string.

        Prior to rendering, this method will convert a template that is a
        byte string (type str in Python 2) to unicode using the string_encoding
        and decode_errors attributes.  See the constructor docstring for
        more information.

        Arguments:

          template: a template string that is unicode or a byte string,
            a ParsedTemplate instance, or another object instance.  In the
            final case, the function first looks for the template associated
            to the object by calling this class's get_associated_template()
            method.  The rendering process also uses the passed object as
            the first element of the context stack when rendering.

          *context: zero or more dictionaries, ContextStack instances, or objects
            with which to populate the initial context stack.  None
            arguments are skipped.  Items in the *context list are added to
            the context stack in order so that later items in the argument
            list take precedence over earlier items.

          **kwargs: additional key-value data to add to the context stack.
            As these arguments appear after all items in the *context list,
            in the case of key conflicts these values take precedence over
            all items in the *context list.

        """
        if is_string(template):
            return self._render_string(template, *context, **kwargs)
        if isinstance(template, ParsedTemplate):
            render_func = lambda engine, stack: template.render(engine, stack)
            return self._render_final(render_func, *context, **kwargs)
        # Otherwise, we assume the template is an object.

        return self._render_object(template, *context, **kwargs)
