from typing import Any

from . import ast
from .errors import ParseError
from .parser import FluentParser
from .serializer import FluentSerializer
from .stream import FluentParserStream
from .visitor import Transformer, Visitor

__all__ = [
    'FluentParser',
    'FluentParserStream',
    'FluentSerializer',
    'ParseError',
    'Transformer',
    'Visitor',
    'ast',
    'parse',
    'serialize'
]


def parse(source: str, **kwargs: Any) -> ast.Resource:
    """Create an ast.Resource from a Fluent Syntax source.
    """
    parser = FluentParser(**kwargs)
    return parser.parse(source)


def serialize(resource: ast.Resource, **kwargs: Any) -> str:
    """Serialize an ast.Resource to a unicode string.
    """
    serializer = FluentSerializer(**kwargs)
    return serializer.serialize(resource)
