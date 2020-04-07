"""List of functions which are useful in many places."""

import typing


T = typing.TypeVar("T")


def keep_until(
        iterable: typing.Iterable[T],
        pred: typing.Callable[[T], bool]
) -> typing.Iterable[T]:
    """Filter an iterable generator or list and keep all elements until the first
    time the predicate becomes true, including the element where the predicate
    is true. All elements after are skipped."""
    for e in iterable:
        yield e
        if pred(e):
            return
