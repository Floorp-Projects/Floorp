"""Emit code and parser tables in Python and Rust."""

__all__ = ['write_python_parser', 'write_rust_parser']

from .python import write_python_parser
from .rust import write_rust_parser
