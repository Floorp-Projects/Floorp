from .parser import FluentParser
from .serializer import FluentSerializer


def parse(source, **kwargs):
    """Create an ast.Resource from a Fluent Syntax source.
    """
    parser = FluentParser(**kwargs)
    return parser.parse(source)


def serialize(resource, **kwargs):
    """Serialize an ast.Resource to a unicode string.
    """
    serializer = FluentSerializer(**kwargs)
    return serializer.serialize(resource)
