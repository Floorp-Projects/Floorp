from voluptuous import Invalid, MultipleInvalid
from voluptuous.error import Error


MAX_VALIDATION_ERROR_ITEM_LENGTH = 500


def _nested_getitem(data, path):
    for item_index in path:
        try:
            data = data[item_index]
        except (KeyError, IndexError, TypeError):
            # The index is not present in the dictionary, list or other
            # indexable or data is not subscriptable
            return None
    return data


def humanize_error(data, validation_error, max_sub_error_length=MAX_VALIDATION_ERROR_ITEM_LENGTH):
    """ Provide a more helpful + complete validation error message than that provided automatically
    Invalid and MultipleInvalid do not include the offending value in error messages,
    and MultipleInvalid.__str__ only provides the first error.
    """
    if isinstance(validation_error, MultipleInvalid):
        return '\n'.join(sorted(
            humanize_error(data, sub_error, max_sub_error_length)
            for sub_error in validation_error.errors
        ))
    else:
        offending_item_summary = repr(_nested_getitem(data, validation_error.path))
        if len(offending_item_summary) > max_sub_error_length:
            offending_item_summary = offending_item_summary[:max_sub_error_length - 3] + '...'
        return '%s. Got %s' % (validation_error, offending_item_summary)


def validate_with_humanized_errors(data, schema, max_sub_error_length=MAX_VALIDATION_ERROR_ITEM_LENGTH):
    try:
        return schema(data)
    except (Invalid, MultipleInvalid) as e:
        raise Error(humanize_error(data, e, max_sub_error_length))
