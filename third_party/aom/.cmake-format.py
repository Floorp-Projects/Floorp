# Generated with cmake-format 0.3.6
# How wide to allow formatted cmake files
line_width = 80

# How many spaces to tab for indent
tab_size = 2

# If arglists are longer than this, break them always. This introduces some
# interesting effects with complicated 'if' statements. However, we want file
# lists to look reasonable. Try to strike a balance.
max_subargs_per_line = 10

# If true, separate flow control names from their parentheses with a space
separate_ctrl_name_with_space = False

# If true, separate function names from parentheses with a space
separate_fn_name_with_space = False

# If a statement is wrapped to more than one line, than dangle the closing
# parenthesis on it's own line
dangle_parens = False

# What character to use for bulleted lists
bullet_char = u'*'

# What character to use as punctuation after numerals in an enumerated list
enum_char = u'.'

# What style line endings to use in the output.
line_ending = u'unix'

# Format command names consistently as 'lower' or 'upper' case
command_case = u'lower'

# Specify structure for custom cmake functions
additional_commands = {
  "foo": {
    "flags": [
      "BAR",
      "BAZ"
    ],
    "kwargs": {
      "HEADERS": "*",
      "DEPENDS": "*",
      "SOURCES": "*"
    }
  }
}
