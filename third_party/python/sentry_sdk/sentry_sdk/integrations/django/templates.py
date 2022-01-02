from django.template import TemplateSyntaxError

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import Dict
    from typing import Optional
    from typing import Iterator
    from typing import Tuple

try:
    # support Django 1.9
    from django.template.base import Origin
except ImportError:
    # backward compatibility
    from django.template.loader import LoaderOrigin as Origin


def get_template_frame_from_exception(exc_value):
    # type: (Optional[BaseException]) -> Optional[Dict[str, Any]]

    # As of Django 1.9 or so the new template debug thing showed up.
    if hasattr(exc_value, "template_debug"):
        return _get_template_frame_from_debug(exc_value.template_debug)  # type: ignore

    # As of r16833 (Django) all exceptions may contain a
    # ``django_template_source`` attribute (rather than the legacy
    # ``TemplateSyntaxError.source`` check)
    if hasattr(exc_value, "django_template_source"):
        return _get_template_frame_from_source(
            exc_value.django_template_source  # type: ignore
        )

    if isinstance(exc_value, TemplateSyntaxError) and hasattr(exc_value, "source"):
        source = exc_value.source
        if isinstance(source, (tuple, list)) and isinstance(source[0], Origin):
            return _get_template_frame_from_source(source)  # type: ignore

    return None


def _get_template_frame_from_debug(debug):
    # type: (Dict[str, Any]) -> Dict[str, Any]
    if debug is None:
        return None

    lineno = debug["line"]
    filename = debug["name"]
    if filename is None:
        filename = "<django template>"

    pre_context = []
    post_context = []
    context_line = None

    for i, line in debug["source_lines"]:
        if i < lineno:
            pre_context.append(line)
        elif i > lineno:
            post_context.append(line)
        else:
            context_line = line

    return {
        "filename": filename,
        "lineno": lineno,
        "pre_context": pre_context[-5:],
        "post_context": post_context[:5],
        "context_line": context_line,
        "in_app": True,
    }


def _linebreak_iter(template_source):
    # type: (str) -> Iterator[int]
    yield 0
    p = template_source.find("\n")
    while p >= 0:
        yield p + 1
        p = template_source.find("\n", p + 1)


def _get_template_frame_from_source(source):
    # type: (Tuple[Origin, Tuple[int, int]]) -> Optional[Dict[str, Any]]
    if not source:
        return None

    origin, (start, end) = source
    filename = getattr(origin, "loadname", None)
    if filename is None:
        filename = "<django template>"
    template_source = origin.reload()
    lineno = None
    upto = 0
    pre_context = []
    post_context = []
    context_line = None

    for num, next in enumerate(_linebreak_iter(template_source)):
        line = template_source[upto:next]
        if start >= upto and end <= next:
            lineno = num
            context_line = line
        elif lineno is None:
            pre_context.append(line)
        else:
            post_context.append(line)

        upto = next

    if context_line is None or lineno is None:
        return None

    return {
        "filename": filename,
        "lineno": lineno,
        "pre_context": pre_context[-5:],
        "post_context": post_context[:5],
        "context_line": context_line,
    }
