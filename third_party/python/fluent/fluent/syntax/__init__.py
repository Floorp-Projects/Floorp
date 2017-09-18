from .parser import FluentParser
from .serializer import FluentSerializer


def parse(source, **kwargs):
    parser = FluentParser(**kwargs)
    return parser.parse(source)


def serialize(resource, **kwargs):
    serializer = FluentSerializer(**kwargs)
    return serializer.serialize(resource)
