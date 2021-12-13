# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class MismatchedRowLengthsException(Exception):
    """
    This exception is thrown when there is a mismatch between the number of items in a row,
    and the number of headers defined.
    """

    pass


class TableBuilder(object):
    """
    Helper class for building tables.
    """

    def __init__(self, title, widths, header_rows, headers, indent=0):
        """
        :param title: str - Title of the table
        :param widths: list of str - Widths of each column of the table
        :param header_rows: int - Number of header rows
        :param headers: 2D list of str - Headers
        :param indent: int - Number of spaces to indent table
        """
        if not isinstance(title, str):
            raise TypeError("TableBuilder attribute title must be a string.")
        if not isinstance(widths, list) or not isinstance(widths[0], int):
            raise TypeError("TableBuilder attribute widths must be a list of integers.")
        if not isinstance(header_rows, int):
            raise TypeError("TableBuilder attribute header_rows must be an integer.")
        if (
            not isinstance(headers, list)
            or not isinstance(headers[0], list)
            or not isinstance(headers[0][0], str)
        ):
            raise TypeError(
                "TableBuilder attribute headers must be a two-dimensional list of strings."
            )
        if not isinstance(indent, int):
            raise TypeError("TableBuilder attribute indent must be an integer.")

        self.title = title
        self.widths = widths
        self.header_rows = header_rows
        self.headers = headers
        self.indent = " " * indent
        self.table = ""
        self._build_table()

    def _build_table(self):
        if len(self.widths) != len(self.headers[0]):
            raise MismatchedRowLengthsException(
                "Number of table headers must match number of column widths."
            )
        widths = " ".join(map(str, self.widths))
        self.table += (
            f"{self.indent}.. list-table:: **{self.title}**\n"
            f"{self.indent}   :widths: {widths}\n"
            f"{self.indent}   :header-rows: {self.header_rows}\n\n"
        )
        self.add_rows(self.headers)

    def add_rows(self, rows):
        if type(rows) != list or type(rows[0]) != list or type(rows[0][0]) != str:
            raise TypeError("add_rows() requires a two-dimensional list of strings.")
        for row in rows:
            self.add_row(row)

    def add_row(self, values):
        if len(values) != len(self.widths):
            raise MismatchedRowLengthsException(
                "Number of items in a row must must number of columns defined."
            )
        for i, val in enumerate(values):
            if i == 0:
                self.table += f"{self.indent}   * - **{val}**\n"
            else:
                self.table += f"{self.indent}     - {val}\n"

    def finish_table(self):
        self.table += "\n"
        return self.table
