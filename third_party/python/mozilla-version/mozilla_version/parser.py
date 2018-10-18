"""Defines parser helpers."""

from mozilla_version.errors import MissingFieldError


def get_value_matched_by_regex(field_name, regex_matches, string):
    """Ensure value stored in regex group exists."""
    try:
        value = regex_matches.group(field_name)
        if value is not None:
            return value
    except IndexError:
        pass

    raise MissingFieldError(string, field_name)
