# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib


class PerfDocLogger(object):
    """
    Logger for the PerfDoc tooling. Handles the warnings by outputting
    them into through the StructuredLogger provided by lint.
    """

    TOP_DIR = ""
    PATHS = []
    LOGGER = None
    FAILED = False

    def __init__(self):
        """Initializes the PerfDocLogger."""

        # Set up class attributes for all logger instances
        if not PerfDocLogger.LOGGER:
            raise Exception(
                "Missing linting LOGGER instance for PerfDocLogger initialization"
            )
        if not PerfDocLogger.PATHS:
            raise Exception("Missing PATHS for PerfDocLogger initialization")
        self.logger = PerfDocLogger.LOGGER

    def log(self, msg):
        """
        Log an info message.

        :param str msg: Message to log.
        """
        self.logger.info(msg)

    def warning(self, msg, files, restricted=True):
        """
        Logs a validation warning message. The warning message is
        used as the error message that is output in the reviewbot.

        :param str msg: Message to log, it's also used as the error message
            for the issue that is output by the reviewbot.
        :param list/str files: The file(s) that this warning is about.
        :param boolean restricted: If the param is False, the lint error can be used anywhere.
        """
        if type(files) != list:
            files = [files]

        if len(files) == 0:
            self.logger.info("No file was provided for the warning")
            self.logger.lint_error(
                message=msg,
                lineno=0,
                column=None,
                path=None,
                linter="perfdocs",
                rule="Flawless performance docs (unknown file)",
            )

            PerfDocLogger.FAILED = True
            return

        # Add a reviewbot error for each file that is given
        for file in files:
            # Get a relative path (reviewbot can't handle absolute paths)
            fpath = str(file).replace(str(PerfDocLogger.TOP_DIR), "")

            # Filter out any issues that do not relate to the paths
            # that are being linted
            for path in PerfDocLogger.PATHS:
                if restricted and str(path) not in str(file):
                    continue

                # Output error entry
                self.logger.lint_error(
                    message=msg,
                    lineno=0,
                    column=None,
                    path=str(pathlib.PurePosixPath(fpath)),
                    linter="perfdocs",
                    rule="Flawless performance docs.",
                )

                PerfDocLogger.FAILED = True
                break

    def critical(self, msg):
        """
        Log a critical message.

        :param str msg: Message to log.
        """
        self.logger.critical(msg)
