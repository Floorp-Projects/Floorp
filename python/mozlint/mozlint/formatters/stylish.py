# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozterm import Terminal

from ..result import Issue
from ..util.string import pluralize


class StylishFormatter(object):
    """Formatter based on the eslint default."""

    _indent_ = "  "

    # Colors later on in the list are fallbacks in case the terminal
    # doesn't support colors earlier in the list.
    # See http://www.calmar.ws/vim/256-xterm-24bit-rgb-color-chart.html
    _colors = {
        "grey": [247, 8, 7],
        "red": [1],
        "green": [2],
        "yellow": [3],
        "brightred": [9, 1],
        "brightyellow": [11, 3],
    }

    fmt = """
  {c1}{lineno}{column}  {c2}{level}{normal}  {message}  {c1}{rule}({linter}){normal}
{diff}""".lstrip(
        "\n"
    )
    fmt_summary = (
        "{t.bold}{c}\u2716 {problem} ({error}, {warning}{failure}, {fixed}){t.normal}"
    )

    def __init__(self, disable_colors=False):
        self.term = Terminal(disable_styling=disable_colors)
        self.num_colors = self.term.number_of_colors

    def color(self, color):
        for num in self._colors[color]:
            if num < self.num_colors:
                return self.term.color(num)
        return ""

    def _reset_max(self):
        self.max_lineno = 0
        self.max_column = 0
        self.max_level = 0
        self.max_message = 0

    def _update_max(self, err):
        """Calculates the longest length of each token for spacing."""
        self.max_lineno = max(self.max_lineno, len(str(err.lineno)))
        if err.column:
            self.max_column = max(self.max_column, len(str(err.column)))
        self.max_level = max(self.max_level, len(str(err.level)))
        self.max_message = max(self.max_message, len(err.message))

    def _get_colored_diff(self, diff):
        if not diff:
            return ""

        new_diff = ""
        for line in diff.split("\n"):
            if line.startswith("+"):
                new_diff += self.color("green")
            elif line.startswith("-"):
                new_diff += self.color("red")
            else:
                new_diff += self.term.normal
            new_diff += self._indent_ + line + "\n"
        return new_diff

    def __call__(self, result):
        message = []
        failed = result.failed

        num_errors = 0
        num_warnings = 0
        num_fixed = result.fixed
        for path, errors in sorted(result.issues.items()):
            self._reset_max()

            message.append(self.term.underline(path))
            # Do a first pass to calculate required padding
            for err in errors:
                assert isinstance(err, Issue)
                self._update_max(err)
                if err.level == "error":
                    num_errors += 1
                else:
                    num_warnings += 1

            for err in sorted(
                errors, key=lambda e: (int(e.lineno), int(e.column or 0))
            ):
                if err.column:
                    col = ":" + str(err.column).ljust(self.max_column)
                else:
                    col = "".ljust(self.max_column + 1)

                args = {
                    "normal": self.term.normal,
                    "c1": self.color("grey"),
                    "c2": self.color("red")
                    if err.level == "error"
                    else self.color("yellow"),
                    "lineno": str(err.lineno).rjust(self.max_lineno),
                    "column": col,
                    "level": err.level.ljust(self.max_level),
                    "rule": "{} ".format(err.rule) if err.rule else "",
                    "linter": err.linter.lower(),
                    "message": err.message.ljust(self.max_message),
                    "diff": self._get_colored_diff(err.diff).ljust(self.max_message),
                }
                message.append(self.fmt.format(**args).rstrip().rstrip("\n"))

            message.append("")  # newline

        # If there were failures, make it clear which linters failed
        for fail in failed:
            message.append(
                "{c}A failure occurred in the {name} linter.".format(
                    c=self.color("brightred"), name=fail
                )
            )

        # Print a summary
        message.append(
            self.fmt_summary.format(
                t=self.term,
                c=self.color("brightred")
                if num_errors or failed
                else self.color("brightyellow"),
                problem=pluralize("problem", num_errors + num_warnings + len(failed)),
                error=pluralize("error", num_errors),
                warning=pluralize(
                    "warning", num_warnings or result.total_suppressed_warnings
                ),
                failure=", {}".format(pluralize("failure", len(failed)))
                if failed
                else "",
                fixed="{} fixed".format(num_fixed),
            )
        )

        if result.total_suppressed_warnings > 0 and num_errors == 0:
            message.append(
                "(pass {c1}-W/--warnings{c2} to see warnings.)".format(
                    c1=self.color("grey"), c2=self.term.normal
                )
            )

        return "\n".join(message)
