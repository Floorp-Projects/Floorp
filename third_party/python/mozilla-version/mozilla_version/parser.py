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


def does_regex_have_group(regex_matches, group_name):
    """Return a boolean depending on whether a regex group is matched."""
    try:
        return regex_matches.group(group_name) is not None
    except IndexError:
        return False


def positive_int(val):
    """Parse `val` into a positive integer."""
    if isinstance(val, float):
        raise ValueError('"{}" must not be a float'.format(val))
    val = int(val)
    if val >= 0:
        return val
    raise ValueError('"{}" must be positive'.format(val))


def positive_int_or_none(val):
    """Parse `val` into either `None` or a positive integer."""
    if val is None:
        return val
    return positive_int(val)


def strictly_positive_int_or_none(val):
    """Parse `val` into either `None` or a strictly positive integer."""
    val = positive_int_or_none(val)
    if val is None or val > 0:
        return val
    raise ValueError('"{}" must be strictly positive'.format(val))
