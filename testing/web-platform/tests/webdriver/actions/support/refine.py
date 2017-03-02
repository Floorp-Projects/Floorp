def get_keys(input_el):
    """Get printable characters entered into `input_el`.

    :param input_el: HTML input element.
    """
    rv = input_el.property("value")
    if rv is None:
        return ""
    else:
        return rv


def filter_dict(source, d):
    """Filter `source` dict to only contain same keys as `d` dict.

    :param source: dictionary to filter.
    :param d: dictionary whose keys determine the filtering.
    """
    return {k: source[k] for k in d.keys()}
