# coding: utf-8
from __future__ import absolute_import, division, print_function, unicode_literals

import contextlib
import logging
import sys

from . import click

# Initialise the builtin logging module for other component using it.
# Ex: pip
logging.basicConfig()


class LogContext(object):
    stream = sys.stderr

    def __init__(self, verbosity=0, indent_width=2):
        self.verbosity = verbosity
        self.current_indent = 0
        self._indent_width = indent_width

    def log(self, message, *args, **kwargs):
        kwargs.setdefault("err", True)
        prefix = " " * self.current_indent
        click.secho(prefix + message, *args, **kwargs)

    def debug(self, *args, **kwargs):
        if self.verbosity >= 1:
            self.log(*args, **kwargs)

    def info(self, *args, **kwargs):
        if self.verbosity >= 0:
            self.log(*args, **kwargs)

    def warning(self, *args, **kwargs):
        kwargs.setdefault("fg", "yellow")
        self.log(*args, **kwargs)

    def error(self, *args, **kwargs):
        kwargs.setdefault("fg", "red")
        self.log(*args, **kwargs)

    def _indent(self):
        self.current_indent += self._indent_width

    def _dedent(self):
        self.current_indent -= self._indent_width

    @contextlib.contextmanager
    def indentation(self):
        """
        Increase indentation.
        """
        self._indent()
        try:
            yield
        finally:
            self._dedent()


log = LogContext()
