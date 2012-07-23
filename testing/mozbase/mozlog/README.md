[Mozlog](https://github.com/mozilla/mozbase/tree/master/mozlog)
is a python package intended to simplify and standardize logs in the Mozilla universe.
It wraps around python's [logging](http://docs.python.org/library/logging.html)
module and adds some additional functionality.

# Usage

Import mozlog instead of [logging](http://docs.python.org/library/logging.html)
(all functionality in the logging module is also available from the mozlog module).
To get a logger, call mozlog.getLogger passing in a name and the path to a log file.
If no log file is specified, the logger will log to stdout.

    import mozlog
    logger = mozlog.getLogger('LOG_NAME', 'log_file_path')
    logger.setLevel(mozlog.DEBUG)
    logger.info('foo')
    logger.testPass('bar')
    mozlog.shutdown()
