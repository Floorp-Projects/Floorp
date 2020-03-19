"""List of functions which are useful in many places."""

def keep_until(iterable, pred):
    """Filter an iterable generator or list and keep all elements until the first
    time the predicate becomes true, including the element where the predicate
    is true. All elements after are skipped."""
    for e in iterable:
        yield e
        if pred(e):
            return
