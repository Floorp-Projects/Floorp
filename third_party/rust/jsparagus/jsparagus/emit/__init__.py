"""Emit code and parser tables in Python and Rust."""

__all__ = ['write_python_parse_table', 'write_rust_parse_table']

from .python import write_python_parse_table
from .rust import write_rust_parse_table
