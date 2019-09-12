#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Generic logging classes and functionalities for single and multi file logging.
Capturing console output and providing general logging functionalities.

Attributes:
    FATAL_LEVEL (int): constant logging level value set based on the logging.CRITICAL
                       value
    DEBUG (str): mozharness `debug` log name
    INFO (str): mozharness `info` log name
    WARNING (str): mozharness `warning` log name
    CRITICAL (str): mozharness `critical` log name
    FATAL (str): mozharness `fatal` log name
    IGNORE (str): mozharness `ignore` log name
    LOG_LEVELS (dict): mapping of the mozharness log level names to logging values
    ROOT_LOGGER (logging.Logger): instance of a logging.Logger class

TODO:
- network logging support.
- log rotation config
"""

from __future__ import print_function

from datetime import datetime
import logging
import os
import sys
import time
import traceback

# Define our own FATAL_LEVEL
FATAL_LEVEL = logging.CRITICAL + 10
logging.addLevelName(FATAL_LEVEL, 'FATAL')

# mozharness log levels.
DEBUG, INFO, WARNING, ERROR, CRITICAL, FATAL, IGNORE = (
    'debug', 'info', 'warning', 'error', 'critical', 'fatal', 'ignore')


LOG_LEVELS = {
    DEBUG: logging.DEBUG,
    INFO: logging.INFO,
    WARNING: logging.WARNING,
    ERROR: logging.ERROR,
    CRITICAL: logging.CRITICAL,
    FATAL: FATAL_LEVEL
}

# mozharness root logger
ROOT_LOGGER = logging.getLogger()

# Force logging to use UTC timestamps
logging.Formatter.converter = time.gmtime


# LogMixin {{{1
class LogMixin(object):
    """This is a mixin for any object to access similar logging functionality

    The logging functionality described here is specially useful for those
    objects with self.config and self.log_obj member variables
    """

    def _log_level_at_least(self, level):
        """ Check if the current logging level is greater or equal than level

        Args:
            level (str): log level name to compare against mozharness log levels
                         names

        Returns:
            bool: True if the current logging level is great or equal than level,
                  False otherwise
        """
        log_level = INFO
        levels = [DEBUG, INFO, WARNING, ERROR, CRITICAL, FATAL]
        if hasattr(self, 'config'):
            log_level = self.config.get('log_level', INFO)
        return levels.index(level) >= levels.index(log_level)

    def _print(self, message, stderr=False):
        """ prints a message to the sys.stdout or sys.stderr according to the
        value of the stderr argument.

        Args:
            message (str): The message to be printed
            stderr (bool, optional): if True, message will be printed to
                                     sys.stderr. Defaults to False.

        Returns:
            None
        """
        if not hasattr(self, 'config') or self.config.get('log_to_console', True):
            if stderr:
                print(message, file=sys.stderr)
            else:
                print(message)

    def log(self, message, level=INFO, exit_code=-1):
        """ log the message passed to it according to level, exit if level == FATAL

        Args:
            message (str): message to be logged
            level (str, optional): logging level of the message. Defaults to INFO
            exit_code (int, optional): exit code to log before the scripts calls
                                       SystemExit.

        Returns:
            None
        """
        if self.log_obj:
            return self.log_obj.log_message(
                message, level=level,
                exit_code=exit_code,
                post_fatal_callback=self._post_fatal,
            )
        if level == INFO:
            if self._log_level_at_least(level):
                self._print(message)
        elif level == DEBUG:
            if self._log_level_at_least(level):
                self._print('DEBUG: %s' % message)
        elif level in (WARNING, ERROR, CRITICAL):
            if self._log_level_at_least(level):
                self._print("%s: %s" % (level.upper(), message), stderr=True)
        elif level == FATAL:
            if self._log_level_at_least(level):
                self._print("FATAL: %s" % message, stderr=True)
            raise SystemExit(exit_code)

    def worst_level(self, target_level, existing_level, levels=None):
        """Compare target_level with existing_level according to levels values
        and return the worst among them.

        Args:
            target_level (str): minimum logging level to which the current object
                                should be set
            existing_level (str): current logging level
            levels (list(str), optional): list of logging levels names to compare
                                          target_level and existing_level against.
                                          Defaults to mozharness log level
                                          list sorted from most to less critical.

        Returns:
            str: the logging lavel that is closest to the first levels value,
                 i.e. levels[0]
        """
        if not levels:
            levels = [FATAL, CRITICAL, ERROR, WARNING, INFO, DEBUG, IGNORE]
        if target_level not in levels:
            self.fatal("'%s' not in %s'." % (target_level, levels))
        for l in levels:
            if l in (target_level, existing_level):
                return l

    # Copying Bear's dumpException():
    # https://hg.mozilla.org/build/tools/annotate/1485f23c38e0/sut_tools/sut_lib.py#l23
    def exception(self, message=None, level=ERROR):
        """ log an exception message base on the log level passed to it.

        This function fetches the information of the current exception being handled and
        adds it to the message argument.

        Args:
            message (str, optional): message to be printed at the beginning of the log.
                                     Default to an empty string.
            level (str, optional): log level to use for the logging. Defaults to ERROR

        Returns:
            None
        """
        tb_type, tb_value, tb_traceback = sys.exc_info()
        if message is None:
            message = ""
        else:
            message = "%s\n" % message
        for s in traceback.format_exception(tb_type, tb_value, tb_traceback):
            message += "%s\n" % s
        # Log at the end, as a fatal will attempt to exit after the 1st line.
        self.log(message, level=level)

    def debug(self, message):
        """ calls the log method with DEBUG as logging level

        Args:
            message (str): message to log
        """
        self.log(message, level=DEBUG)

    def info(self, message):
        """ calls the log method with INFO as logging level

        Args:
            message (str): message to log
        """
        self.log(message, level=INFO)

    def warning(self, message):
        """ calls the log method with WARNING as logging level

        Args:
            message (str): message to log
        """
        self.log(message, level=WARNING)

    def error(self, message):
        """ calls the log method with ERROR as logging level

        Args:
            message (str): message to log
        """
        self.log(message, level=ERROR)

    def critical(self, message):
        """ calls the log method with CRITICAL as logging level

        Args:
            message (str): message to log
        """
        self.log(message, level=CRITICAL)

    def fatal(self, message, exit_code=-1):
        """ calls the log method with FATAL as logging level

        Args:
            message (str): message to log
            exit_code (int, optional): exit code to use for the SystemExit
                                       exception to be raised. Default to -1.
        """
        self.log(message, level=FATAL, exit_code=exit_code)

    def _post_fatal(self, message=None, exit_code=None):
        """ Sometimes you want to create a report or cleanup
        or notify on fatal(); override this method to do so.

        Please don't use this for anything significantly long-running.

        Args:
            message (str, optional): message to report. Default to None
            exit_code (int, optional): exit code to use for the SystemExit
                                       exception to be raised. Default to None
        """
        pass


# OutputParser {{{1
class OutputParser(LogMixin):
    """ Helper object to parse command output.

    This will buffer output if needed, so we can go back and mark
    [(linenum - 10) : linenum+10] as errors if need be, without having to
    get all the output first.

    linenum+10 will be easy; we can set self.num_post_context_lines to 10,
    and self.num_post_context_lines-- as we mark each line to at least error
    level X.

    linenum-10 will be trickier. We'll not only need to save the line
    itself, but also the level that we've set for that line previously,
    whether by matching on that line, or by a previous line's context.
    We should only log that line if all output has ended (self.finish() ?);
    otherwise store a list of dictionaries in self.context_buffer that is
    buffered up to self.num_pre_context_lines (set to the largest
    pre-context-line setting in error_list.)
    """

    def __init__(self, config=None, log_obj=None, error_list=None, log_output=True, **kwargs):
        """Initialization method for the OutputParser class

        Args:
            config (dict, optional): dictionary containing values such as `log_level`
                                    or `log_to_console`. Defaults to `None`.
            log_obj (BaseLogger, optional): instance of the BaseLogger class. Defaults
                                            to `None`.
            error_list (list, optional): list of the error to look for. Defaults to
                                        `None`.
            log_output (boolean, optional): flag for deciding if the commands
                                            output should be logged or not.
                                            Defaults to `True`.
        """
        self.config = config
        self.log_obj = log_obj
        self.error_list = error_list or []
        self.log_output = log_output
        self.num_errors = 0
        self.num_warnings = 0
        # TODO context_lines.
        # Not in use yet, but will be based off error_list.
        self.context_buffer = []
        self.num_pre_context_lines = 0
        self.num_post_context_lines = 0
        self.worst_log_level = INFO

    def parse_single_line(self, line):
        """ parse a console output line and check if it matches one in `error_list`,
        if so then log it according to `log_output`.

        Args:
            line (str): command line output to parse.

        Returns:
            If the line hits a match in the error_list, the new log level the line was
            (or should be) logged at is returned. Otherwise, returns None.
        """
        for error_check in self.error_list:
            # TODO buffer for context_lines.
            match = False
            if 'substr' in error_check:
                if error_check['substr'] in line:
                    match = True
            elif 'regex' in error_check:
                if error_check['regex'].search(line):
                    match = True
            else:
                self.warning("error_list: 'substr' and 'regex' not in %s" %
                             error_check)
            if match:
                log_level = error_check.get('level', INFO)
                if self.log_output:
                    message = ' %s' % line
                    if error_check.get('explanation'):
                        message += '\n %s' % error_check['explanation']
                    if error_check.get('summary'):
                        self.add_summary(message, level=log_level)
                    else:
                        self.log(message, level=log_level)
                if log_level in (ERROR, CRITICAL, FATAL):
                    self.num_errors += 1
                if log_level == WARNING:
                    self.num_warnings += 1
                self.worst_log_level = self.worst_level(log_level,
                                                        self.worst_log_level)
                return log_level

        if self.log_output:
            self.info(' %s' % line)

    def add_lines(self, output):
        """ process a string or list of strings, decode them to utf-8,strip
        them of any trailing whitespaces and parse them using `parse_single_line`

        strings consisting only of whitespaces are ignored.

        Args:
            output (str | list): string or list of string to parse
        """

        if isinstance(output, basestring):
            output = [output]
        for line in output:
            if not line or line.isspace():
                continue
            line = line.decode("utf-8", 'replace').rstrip()
            self.parse_single_line(line)


# BaseLogger {{{1
class BaseLogger(object):
    """ Base class in charge of logging handling logic such as creating logging
    files, dirs, attaching to the console output and managing its output.

    Attributes:
        LEVELS (dict): flat copy of the `LOG_LEVELS` attribute of the `log` module.

    TODO: status? There may be a status object or status capability in
    either logging or config that allows you to count the number of
    error,critical,fatal messages for us to count up at the end (aiming
    for 0).
    """
    LEVELS = LOG_LEVELS

    def __init__(
        self, log_level=INFO,
        log_format='%(message)s',
        log_date_format='%H:%M:%S',
        log_name='test',
        log_to_console=True,
        log_dir='.',
        log_to_raw=False,
        logger_name='',
        append_to_log=False,
    ):
        """ BaseLogger constructor

        Args:
            log_level (str, optional): mozharness log level name. Defaults to INFO.
            log_format (str, optional): message format string to instantiate a
                                        `logging.Formatter`. Defaults to '%(message)s'
            log_date_format (str, optional): date format string to instantiate a
                                            `logging.Formatter`. Defaults to '%H:%M:%S'
            log_name (str, optional): name to use for the log files to be created.
                                      Defaults to 'test'
            log_to_console (bool, optional): set to True in order to create a Handler
                                             instance base on the `Logger`
                                             current instance. Defaults to True.
            log_dir (str, optional): directory location to store the log files.
                                     Defaults to '.', i.e. current working directory.
            log_to_raw (bool, optional): set to True in order to create a *raw.log
                                         file. Defaults to False.
            logger_name (str, optional): currently useless parameter. According
                                         to the code comments, it could be useful
                                         if we were to have multiple logging
                                         objects that don't trample each other.
            append_to_log (bool, optional): set to True if the logging content should
                                            be appended to old logging files. Defaults to False
        """

        self.log_format = log_format
        self.log_date_format = log_date_format
        self.log_to_console = log_to_console
        self.log_to_raw = log_to_raw
        self.log_level = log_level
        self.log_name = log_name
        self.log_dir = log_dir
        self.append_to_log = append_to_log

        # Not sure what I'm going to use this for; useless unless we
        # can have multiple logging objects that don't trample each other
        self.logger_name = logger_name

        self.all_handlers = []
        self.log_files = {}

        self.create_log_dir()

    def create_log_dir(self):
        """ create a logging directory if it doesn't exits. If there is a file with
        same name as the future logging directory it will be deleted.
        """

        if os.path.exists(self.log_dir):
            if not os.path.isdir(self.log_dir):
                os.remove(self.log_dir)
        if not os.path.exists(self.log_dir):
            os.makedirs(self.log_dir)
        self.abs_log_dir = os.path.abspath(self.log_dir)

    def init_message(self, name=None):
        """ log an init message stating the name passed to it, the current date
        and time and, the current working directory.

        Args:
            name (str, optional): name to use for the init log message. Defaults to
                                  the current instance class name.
        """

        if not name:
            name = self.__class__.__name__
        self.log_message("%s online at %s in %s" %
                         (name, datetime.utcnow().strftime("%Y%m%d %H:%M:%SZ"),
                          os.getcwd()))

    def get_logger_level(self, level=None):
        """ translate the level name passed to it and return its numeric value
            according to `LEVELS` values.

        Args:
            level (str, optional): level name to be translated. Defaults to the current
                                   instance `log_level`.

        Returns:
            int: numeric value of the log level name passed to it or 0 (NOTSET) if the
                 name doesn't exists
        """

        if not level:
            level = self.log_level
        return self.LEVELS.get(level, logging.NOTSET)

    def get_log_formatter(self, log_format=None, date_format=None):
        """ create a `logging.Formatter` base on the log and date format.

        Args:
            log_format (str, optional): log format to use for the Formatter constructor.
                                        Defaults to the current instance log format.
            date_format (str, optional): date format to use for the Formatter constructor.
                                         Defaults to the current instance date format.

        Returns:
            logging.Formatter: instance created base on the passed arguments
        """

        if not log_format:
            log_format = self.log_format
        if not date_format:
            date_format = self.log_date_format
        return logging.Formatter(log_format, date_format)

    def new_logger(self):
        """ Create a new logger based on the ROOT_LOGGER instance. By default there are no handlers.
            The new logger becomes a member variable of the current instance as `self.logger`.
        """

        self.logger = ROOT_LOGGER
        self.logger.setLevel(self.get_logger_level())
        self._clear_handlers()
        if self.log_to_console:
            self.add_console_handler()
        if self.log_to_raw:
            self.log_files['raw'] = '%s_raw.log' % self.log_name
            self.add_file_handler(os.path.join(self.abs_log_dir,
                                               self.log_files['raw']),
                                  log_format='%(message)s')

    def _clear_handlers(self):
        """ remove all handlers stored in `self.all_handlers`.

        To prevent dups -- logging will preserve Handlers across
        objects :(
        """
        attrs = dir(self)
        if 'all_handlers' in attrs and 'logger' in attrs:
            for handler in self.all_handlers:
                self.logger.removeHandler(handler)
            self.all_handlers = []

    def __del__(self):
        """ BaseLogger class destructor; shutdown, flush and remove all handlers"""
        logging.shutdown()
        self._clear_handlers()

    def add_console_handler(self, log_level=None, log_format=None,
                            date_format=None):
        """ create a `logging.StreamHandler` using `sys.stderr` for logging the console
        output and add it to the `all_handlers` member variable

        Args:
            log_level (str, optional): useless argument. Not used here.
                                       Defaults to None.
            log_format (str, optional): format used for the Formatter attached to the
                                        StreamHandler. Defaults to None.
            date_format (str, optional): format used for the Formatter attached to the
                                         StreamHandler. Defaults to None.
        """

        console_handler = logging.StreamHandler()
        console_handler.setFormatter(self.get_log_formatter(log_format=log_format,
                                                            date_format=date_format))
        self.logger.addHandler(console_handler)
        self.all_handlers.append(console_handler)

    def add_file_handler(self, log_path, log_level=None, log_format=None,
                         date_format=None):
        """ create a `logging.FileHandler` base on the path, log and date format
        and add it to the `all_handlers` member variable.

        Args:
            log_path (str): filepath to use for the `FileHandler`.
            log_level (str, optional): useless argument. Not used here.
                                       Defaults to None.
            log_format (str, optional): log format to use for the Formatter constructor.
                                        Defaults to the current instance log format.
            date_format (str, optional): date format to use for the Formatter constructor.
                                         Defaults to the current instance date format.
        """

        if not self.append_to_log and os.path.exists(log_path):
            os.remove(log_path)
        file_handler = logging.FileHandler(log_path)
        file_handler.setLevel(self.get_logger_level(log_level))
        file_handler.setFormatter(self.get_log_formatter(log_format=log_format,
                                                         date_format=date_format))
        self.logger.addHandler(file_handler)
        self.all_handlers.append(file_handler)

    def log_message(self, message, level=INFO, exit_code=-1, post_fatal_callback=None):
        """ Generic log method.
        There should be more options here -- do or don't split by line,
        use os.linesep instead of assuming \n, be able to pass in log level
        by name or number.

        Adding the IGNORE special level for runCommand.

        Args:
            message (str): message to log using the current `logger`
            level (str, optional): log level of the message. Defaults to INFO.
            exit_code (int, optional): exit code to use in case of a FATAL level is used.
                                       Defaults to -1.
            post_fatal_callback (function, optional): function to callback in case of
                                                      of a fatal log level. Defaults None.
        """

        if level == IGNORE:
            return
        for line in message.splitlines():
            self.logger.log(self.get_logger_level(level), line)
        if level == FATAL:
            if callable(post_fatal_callback):
                self.logger.log(FATAL_LEVEL, "Running post_fatal callback...")
                post_fatal_callback(message=message, exit_code=exit_code)
            self.logger.log(FATAL_LEVEL, 'Exiting %d' % exit_code)
            raise SystemExit(exit_code)


# SimpleFileLogger {{{1
class SimpleFileLogger(BaseLogger):
    """ Subclass of the BaseLogger.

    Create one logFile.  Possibly also output to the terminal and a raw log
    (no prepending of level or date)
    """

    def __init__(self,
                 log_format='%(asctime)s %(levelname)8s - %(message)s',
                 logger_name='Simple', log_dir='logs', **kwargs):
        """ SimpleFileLogger constructor. Calls its superclass constructor,
        creates a new logger instance and log an init message.

        Args:
            log_format (str, optional): message format string to instantiate a
                                       `logging.Formatter`. Defaults to
                                       '%(asctime)s %(levelname)8s - %(message)s'
            log_name (str, optional): name to use for the log files to be created.
                                      Defaults to 'Simple'
            log_dir (str, optional): directory location to store the log files.
                                     Defaults to 'logs'
            **kwargs: Arbitrary keyword arguments passed to the BaseLogger constructor
        """

        BaseLogger.__init__(self, logger_name=logger_name, log_format=log_format,
                            log_dir=log_dir, **kwargs)
        self.new_logger()
        self.init_message()

    def new_logger(self):
        """ calls the BaseLogger.new_logger method and adds a file handler to it."""

        BaseLogger.new_logger(self)
        self.log_path = os.path.join(self.abs_log_dir, '%s.log' % self.log_name)
        self.log_files['default'] = self.log_path
        self.add_file_handler(self.log_path)


# MultiFileLogger {{{1
class MultiFileLogger(BaseLogger):
    """Subclass of the BaseLogger class. Create a log per log level in log_dir.
    Possibly also output to the terminal and a raw log (no prepending of level or date)
    """

    def __init__(self, logger_name='Multi',
                 log_format='%(asctime)s %(levelname)8s - %(message)s',
                 log_dir='logs', log_to_raw=True, **kwargs):
        """ MultiFileLogger constructor. Calls its superclass constructor,
        creates a new logger instance and log an init message.

        Args:
            log_format (str, optional): message format string to instantiate a
                                       `logging.Formatter`. Defaults to
                                       '%(asctime)s %(levelname)8s - %(message)s'
            log_name (str, optional): name to use for the log files to be created.
                                      Defaults to 'Multi'
            log_dir (str, optional): directory location to store the log files.
                                     Defaults to 'logs'
            log_to_raw (bool, optional): set to True in order to create a *raw.log
                                         file. Defaults to False.
            **kwargs: Arbitrary keyword arguments passed to the BaseLogger constructor
        """

        BaseLogger.__init__(self, logger_name=logger_name,
                            log_format=log_format,
                            log_to_raw=log_to_raw, log_dir=log_dir,
                            **kwargs)

        self.new_logger()
        self.init_message()

    def new_logger(self):
        """ calls the BaseLogger.new_logger method and adds a file handler per
        logging level in the `LEVELS` class attribute.
        """

        BaseLogger.new_logger(self)
        min_logger_level = self.get_logger_level(self.log_level)
        for level in self.LEVELS.keys():
            if self.get_logger_level(level) >= min_logger_level:
                self.log_files[level] = '%s_%s.log' % (self.log_name,
                                                       level)
                self.add_file_handler(os.path.join(self.abs_log_dir,
                                                   self.log_files[level]),
                                      log_level=level)


# ConsoleLogger {{{1
class ConsoleLogger(BaseLogger):
    """Subclass of the BaseLogger.

    Output logs to stderr.
    """

    def __init__(self,
                 log_format='%(levelname)8s - %(message)s',
                 log_date_format='%H:%M:%S',
                 logger_name='Console', **kwargs):
        """ ConsoleLogger constructor. Calls its superclass constructor,
        creates a new logger instance and log an init message.

        Args:
            log_format (str, optional): message format string to instantiate a
                                       `logging.Formatter`. Defaults to
                                       '%(levelname)8s - %(message)s'
            **kwargs: Arbitrary keyword arguments passed to the BaseLogger
                      constructor
        """

        BaseLogger.__init__(self, logger_name=logger_name,
                            log_format=log_format, **kwargs)
        self.new_logger()
        self.init_message()

    def new_logger(self):
        """ Create a new logger based on the ROOT_LOGGER instance. By default
        there are no handlers.  The new logger becomes a member variable of the
        current instance as `self.logger`.
        """
        self.logger = ROOT_LOGGER
        self.logger.setLevel(self.get_logger_level())
        self._clear_handlers()
        self.add_console_handler()


def numeric_log_level(level):
    """Converts a mozharness log level (string) to the corresponding logger
    level (number). This function makes possible to set the log level
    in functions that do not inherit from LogMixin

    Args:
        level (str): log level name to convert.

    Returns:
        int: numeric value of the log level name.
    """
    return LOG_LEVELS[level]


# __main__ {{{1
if __name__ == '__main__':
    """ Useless comparison, due to the `pass` keyword on its body"""
    pass
