import logging
import time

from . import utils

log = logging.getLogger(__name__)


def retry(maxRetries, tryFn):
    """
    Retry `tryFn` based on `maxRetries`.  Each call to `tryFn` will pass a
    callable which should be called with the exception object when an exception
    can be retried.  Exceptions raised from `tryFn` are treated as fatal.
    """

    retry = -1  # we plus first in the loop, and attempt 1 is retry 0
    while True:
        retry += 1

        # if this isn't the first retry then we sleep
        if retry > 0:
            snooze = float(retry * retry) / 10.0
            log.info('Sleeping %0.2f seconds for exponential backoff', snooze)
            time.sleep(utils.calculateSleepTime(retry))

        retriableException = None

        def retryFor(exc):
            nonlocal retriableException
            retriableException = exc

        res = tryFn(retryFor)

        if not retriableException:
            return res

        if retry < maxRetries:
            log.warning(f'Retrying because of: {retriableException}')
            continue

        raise retriableException
