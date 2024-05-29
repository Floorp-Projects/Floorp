"""Defines all errors reported by mozilla-version."""


class PatternNotMatchedError(ValueError):
    """Error when a string doesn't match an expected pattern.

    Args:
        string (str): The string it was unable to match.
        patterns (sequence): The patterns it tried to match.
    """

    def __init__(self, string, patterns):
        """Initialize error."""
        number_of_patterns = len(patterns)
        if number_of_patterns == 0:
            raise ValueError("At least one pattern must be provided")
        if number_of_patterns == 1:
            message = f'"{string}" does not match the pattern: {patterns[0]}'
        else:
            message = '"{}" does not match the patterns:\n - {}'.format(
                string, "\n - ".join(patterns)
            )

        super().__init__(message)


class NoVersionTypeError(ValueError):
    """Error when `version_string` matched the pattern, but was unable to find its type.

    Args:
        version_string (str): The string it was unable to guess the type.
    """

    def __init__(self, version_string):
        """Initialize error."""
        super().__init__(
            f'Version "{version_string}" matched the pattern of a valid version, but '
            "it is unable to find what type it is. This is likely a bug in "
            "mozilla-version"
        )


class MissingFieldError(ValueError):
    """Error when `version_string` lacks an expected field.

    Args:
        version_string (str): The string it was unable to extract a given field.
        field_name (str): The name of the missing field.
    """

    def __init__(self, version_string, field_name):
        """Initialize error."""
        super().__init__(
            f'Release "{version_string}" does not contain a valid {field_name}'
        )


class TooManyTypesError(ValueError):
    """Error when `version_string` has too many types.

    Args:
        version_string (str): The string that gave too many types.
        first_matched_type (str): The name of the first detected type.
        second_matched_type (str): The name of the second detected type
    """

    def __init__(self, version_string, first_matched_type, second_matched_type):
        """Initialize error."""
        super().__init__(
            f'Release "{version_string}" cannot match types "{first_matched_type}" and '
            f'"{second_matched_type}"'
        )
