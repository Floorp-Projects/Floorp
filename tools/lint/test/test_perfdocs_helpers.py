import mozunit
import pytest

LINTER = "perfdocs"

testdata = [
    {
        "table_specifications": {
            "title": ["not a string"],
            "widths": [10, 10, 10, 10],
            "header_rows": 1,
            "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute title must be a string.",
    },
    {
        "table_specifications": {
            "title": "I've got a lovely bunch of coconuts",
            "widths": ("not", "a", "list"),
            "header_rows": 1,
            "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute widths must be a list of integers.",
    },
    {
        "table_specifications": {
            "title": "There they are all standing in a row",
            "widths": ["not an integer"],
            "header_rows": 1,
            "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute widths must be a list of integers.",
    },
    {
        "table_specifications": {
            "title": "Big ones, small ones",
            "widths": [10, 10, 10, 10],
            "header_rows": "not an integer",
            "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute header_rows must be an integer.",
    },
    {
        "table_specifications": {
            "title": "Some as big as your head!",
            "widths": [10, 10, 10, 10],
            "header_rows": 1,
            "headers": ("not", "a", "list"),
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute headers must be a two-dimensional list of strings.",
    },
    {
        "table_specifications": {
            "title": "(And bigger)",
            "widths": [10, 10, 10, 10],
            "header_rows": 1,
            "headers": ["not", "two", "dimensional"],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute headers must be a two-dimensional list of strings.",
    },
    {
        "table_specifications": {
            "title": "Give 'em a twist, a flick of the wrist'",
            "widths": [10, 10, 10, 10],
            "header_rows": 1,
            "headers": [[1, 2, 3]],
            "indent": 2,
        },
        "error_msg": "TableBuilder attribute headers must be a two-dimensional list of strings.",
    },
    {
        "table_specifications": {
            "title": "That's what the showman said!",
            "widths": [10, 10, 10, 10],
            "header_rows": 1,
            "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
            "indent": "not an integer",
        },
        "error_msg": "TableBuilder attribute indent must be an integer.",
    },
]

table_specifications = {
    "title": "I've got a lovely bunch of coconuts",
    "widths": [10, 10, 10],
    "header_rows": 1,
    "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
    "indent": 2,
}


@pytest.mark.parametrize("testdata", testdata)
def test_table_builder_invalid_attributes(testdata):
    from perfdocs.doc_helpers import TableBuilder

    table_specifications = testdata["table_specifications"]
    error_msg = testdata["error_msg"]

    with pytest.raises(TypeError) as error:
        TableBuilder(
            table_specifications["title"],
            table_specifications["widths"],
            table_specifications["header_rows"],
            table_specifications["headers"],
            table_specifications["indent"],
        )

    assert str(error.value) == error_msg


def test_table_builder_mismatched_columns():
    from perfdocs.doc_helpers import MismatchedRowLengthsException, TableBuilder

    table_specifications = {
        "title": "I've got a lovely bunch of coconuts",
        "widths": [10, 10, 10, 42],
        "header_rows": 1,
        "headers": [["Coconut 1", "Coconut 2", "Coconut 3"]],
        "indent": 2,
    }

    with pytest.raises(MismatchedRowLengthsException) as error:
        TableBuilder(
            table_specifications["title"],
            table_specifications["widths"],
            table_specifications["header_rows"],
            table_specifications["headers"],
            table_specifications["indent"],
        )
    assert (
        str(error.value)
        == "Number of table headers must match number of column widths."
    )


def test_table_builder_add_row_too_long():
    from perfdocs.doc_helpers import MismatchedRowLengthsException, TableBuilder

    table = TableBuilder(
        table_specifications["title"],
        table_specifications["widths"],
        table_specifications["header_rows"],
        table_specifications["headers"],
        table_specifications["indent"],
    )
    with pytest.raises(MismatchedRowLengthsException) as error:
        table.add_row(
            ["big ones", "small ones", "some as big as your head!", "(and bigger)"]
        )
    assert (
        str(error.value)
        == "Number of items in a row must must number of columns defined."
    )


def test_table_builder_add_rows_type_error():
    from perfdocs.doc_helpers import TableBuilder

    table = TableBuilder(
        table_specifications["title"],
        table_specifications["widths"],
        table_specifications["header_rows"],
        table_specifications["headers"],
        table_specifications["indent"],
    )
    with pytest.raises(TypeError) as error:
        table.add_rows(
            ["big ones", "small ones", "some as big as your head!", "(and bigger)"]
        )
    assert str(error.value) == "add_rows() requires a two-dimensional list of strings."


def test_table_builder_validate():
    from perfdocs.doc_helpers import TableBuilder

    table = TableBuilder(
        table_specifications["title"],
        table_specifications["widths"],
        table_specifications["header_rows"],
        table_specifications["headers"],
        table_specifications["indent"],
    )
    table.add_row(["big ones", "small ones", "some as big as your head!"])
    table.add_row(
        ["Give 'em a twist", "A flick of the wrist", "That's what the showman said!"]
    )
    table = table.finish_table()
    print(table)
    assert (
        table == "  .. list-table:: **I've got a lovely bunch of coconuts**\n"
        "     :widths: 10 10 10\n     :header-rows: 1\n\n"
        "     * - **Coconut 1**\n       - Coconut 2\n       - Coconut 3\n"
        "     * - **big ones**\n       - small ones\n       - some as big as your head!\n"
        "     * - **Give 'em a twist**\n       - A flick of the wrist\n"
        "       - That's what the showman said!\n\n"
    )


if __name__ == "__main__":
    mozunit.main()
