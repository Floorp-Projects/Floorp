"""List of functions which are useful in many places."""

import sys
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


def consume(iterator: typing.Iterable[T], progress: bool) -> None:
    """Drain the iterator. If progress is true, print dots on stdout."""
    i = 0
    to_feed = str(i)
    try:
        for _ in iterator:
            if progress:
                if len(to_feed) > 0:
                    sys.stdout.write(to_feed[0])
                    to_feed = to_feed[1:]
                else:
                    sys.stdout.write(".")
                i += 1
                if i % 100 == 0:
                    sys.stdout.write("\n")
                    to_feed = str(i)
                sys.stdout.flush()
    finally:
        if progress and i != 0:
            sys.stdout.write("\n")
            sys.stdout.flush()
