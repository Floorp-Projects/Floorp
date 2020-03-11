# OK (tab after = is part of the value)
key01 =	Value 01

# Error (tab before =)
key02	= Value 02

# Error (tab is not a valid indent)
key03 =
	This line isn't properly indented.

# Partial Error (tab is not a valid indent)
key04 =
    This line is indented by 4 spaces,
	whereas this line by 1 tab.
