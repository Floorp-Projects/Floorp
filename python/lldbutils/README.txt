lldb debugging functionality for Gecko
--------------------------------------

This directory contains a module, lldbutils, which is imported by the
in-tree .lldbinit file.  The lldbutil modules define some lldb commands
that are handy for debugging Gecko.

If you want to add a new command or Python-implemented type summary, either add
it to one of the existing broad area Python files (such as lldbutils/layout.py
for layout-related commands) or create a new file if none of the existing files
is appropriate.  If you add a new file, make sure you add it to __all__ in
lldbutils/__init__.py.
