"""Defines all errors reported by mozilla-version."""


class PatternNotMatchedError(ValueError):
    """Error when a string doesn't match an expected pattern.

    Args:
        string (str): The string it was unable to match.
        pattern (str): The pattern used it tried to match.
    """

    def __init__(self, string, pattern):
        """Constructor."""
        super(PatternNotMatchedError, self).__init__(
            '"{}" does not match the pattern: {}'.format(string, pattern)
        )


class NoVersionTypeError(ValueError):
    """Error when `version_string` matched the pattern, but was unable to find its type.

    Args:
        version_string (str): The string it was unable to guess the type.
    """

    def __init__(self, version_string):
        """Constructor."""
        super(NoVersionTypeError, self).__init__(
            'Version "{}" matched the pattern of a valid version, but it is unable to find what type it is. \
This is likely a bug in mozilla-version'.format(version_string)
        )


class MissingFieldError(ValueError):
    """Error when `version_string` lacks an expected field.

    Args:
        version_string (str): The string it was unable to extract a given field.
        field_name (str): The name of the missing field.
    """

    def __init__(self, version_string, field_name):
        """Constructor."""
        super(MissingFieldError, self).__init__(
            'Release "{}" does not contain a valid {}'.format(version_string, field_name)
        )


class TooManyTypesError(ValueError):
    """Error when `version_string` has too many types."""

    def __init__(self, version_string, first_matched_type, second_matched_type):
        """Constructor.

        Args:
            version_string (str): The string that gave too many types.
            first_matched_type (str): The name of the first detected type.
            second_matched_type (str): The name of the second detected type
        """
        super(TooManyTypesError, self).__init__(
            'Release "{}" cannot match types "{}" and "{}"'.format(
                version_string, first_matched_type, second_matched_type
            )
        )
