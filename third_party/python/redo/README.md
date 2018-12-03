
# Redo - Utilities to retry Python callables
******************************************

## Introduction

Redo provides various means to add seamless ability to retry to any Python callable. Redo includes a plain function `(redo.retry)`, a decorator `(redo.retriable)`, and a context manager `(redo.retrying)` to enable you to integrate it in the best possible way for your project. As a bonus, a standalone interface is also included `("retry")`.

## Installation

For installing with pip, run following commands
> pip install redo

## How To Use
Below is the list of functions available
* retrier
* retry
* retriable
* retrying (contextmanager)

### retrier(attempts=5, sleeptime=10, max_sleeptime=300, sleepscale=1.5, jitter=1)
A generator function that sleeps between retries, handles exponential back off and jitter. The action you are retrying is meant to run after retrier yields. At each iteration, we sleep for `sleeptime + random.randint(-jitter, jitter)`. Afterwards sleeptime is multiplied by sleepscale for the next iteration.

**Arguments Detail:**    

1. **attempts (int):** maximum number of times to try; defaults to 5
2. **sleeptime (float):** how many seconds to sleep between tries; defaults to 60s (one minute)
3. **max_sleeptime (float):** the longest we'll sleep, in seconds; defaults to 300s (five minutes)
4. **sleepscale (float):** how much to multiply the sleep time by each iteration; defaults to 1.5
5. **jitter (int):** random jitter to introduce to sleep time each iteration. the amount is chosen at random between `[-jitter, +jitter]` defaults to 1

**Output:**
None, a maximum of `attempts` number of times

**Example:**

    >>> n = 0
    >>> for _ in retrier(sleeptime=0, jitter=0):
    ...     if n == 3:
    ...         # We did the thing!
    ...         break
    ...     n += 1
    >>> n
    3
    >>> n = 0
    >>> for _ in retrier(sleeptime=0, jitter=0):
    ...     if n == 6:
    ...         # We did the thing!
    ...         break
    ...     n += 1
    ... else:
    ...     print("max tries hit")
    max tries hit

### retry(action, attempts=5, sleeptime=60, max_sleeptime=5 * 60, sleepscale=1.5, jitter=1, retry_exceptions=(Exception,), cleanup=None, args=(), kwargs={})  
Calls an action function until it succeeds, or we give up.

**Arguments Detail:**  

1. **action (callable):** the function to retry
2. **attempts (int):** maximum number of times to try; defaults to 5
3. **sleeptime (float):** how many seconds to sleep between tries; defaults to 60s (one minute)
4. **max_sleeptime (float):** the longest we'll sleep, in seconds; defaults to 300s (five minutes)
5. **sleepscale (float):** how much to multiply the sleep time by each iteration; defaults to 1.5
6. **jitter (int):** random jitter to introduce to sleep time each iteration. The amount is chosen at random between `[-jitter, +jitter]` defaults to 1
7. **retry_exceptions (tuple):** tuple of exceptions to be caught. If other exceptions are raised by `action()`, then these are immediately re-raised to the caller.
8. **cleanup (callable):** optional; called if one of `retry_exceptions` is caught. No arguments are passed to the cleanup function; if your cleanup requires arguments, consider using `functools.partial` or a `lambda` function.
9. **args (tuple):** positional arguments to call `action` with
10. **kwargs (dict):** keyword arguments to call `action` with

**Output:**
 Whatever action`(*args, **kwargs)` returns
 
 **Output:**
        Whatever action(*args, **kwargs) raises. `retry_exceptions` are caught
        up until the last attempt, in which case they are re-raised.

**Example:**

    >>> count = 0
    >>> def foo():
    ...     global count
    ...     count += 1
    ...     print(count)
    ...     if count < 3:
    ...         raise ValueError("count is too small!")
    ...     return "success!"
    >>> retry(foo, sleeptime=0, jitter=0)
    1
    2
    3
    'success!'

### retriable(*retry_args, **retry_kwargs)
A decorator factory for `retry()`. Wrap your function in `@retriable(...)` to give it retry powers!

**Arguments Detail:**  
        Same as for `retry`, with the exception of `action`, `args`, and `kwargs`,
        which are left to the normal function definition.

**Output:**
A function decorator

**Example:**

    >>> count = 0
    >>> @retriable(sleeptime=0, jitter=0)
    ... def foo():
    ...     global count
    ...     count += 1
    ...     print(count)
    ...     if count < 3:
    ...         raise ValueError("count too small")
    ...     return "success!"
    >>> foo()
    1
    2
    3
    'success!'

### retrying(func, *retry_args, **retry_kwargs)
A context manager for wrapping functions with retry functionality.

**Arguments Detail:**   

1. **func (callable):** the function to wrap
other arguments as per `retry`

**Output:**
A context manager that returns `retriable(func)` on `__enter__`

**Example:**

    >>> count = 0
    >>> def foo():
    ...     global count
    ...     count += 1
    ...     print(count)
    ...     if count < 3:
    ...         raise ValueError("count too small")
    ...     return "success!"
    >>> with retrying(foo, sleeptime=0, jitter=0) as f:
    ...     f()
    1
    2
    3
    'success!'