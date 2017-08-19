"""Functional testing framework for command line applications"""

from cram._main import main
from cram._test import test, testfile

__all__ = ['main', 'test', 'testfile']
