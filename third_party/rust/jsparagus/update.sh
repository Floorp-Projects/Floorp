#!/bin/bash

# update.sh - Rebuild generated files from parse_pgen.py and pgen.pgen.
#
# These generated files are not actually used to generate themselves,
# so the process isn't as tricky as it could otherwise be. (They are used
# for testing and benchmarking.)
#
# How to change the pgen syntax:
#
# 1. Update the pgen_grammar and ASTBuilder in parse_pgen.py,
#    and other downstream Python and Rust code appropriately.
# 2. Make the corresponding edits to pgen.pgen. You can change it to
#    use the new syntax that you're adding.
# 3. Run this script.
#
# Even if something fails, fear not! It's usually pretty easy to fix stuff and
# get to a fixpoint where everything passes.

set -eu

cd $(dirname "$0")
python3 -m jsparagus.parse_pgen --regenerate > jsparagus/parse_pgen_generated_NEW.py
mv jsparagus/parse_pgen_generated_NEW.py jsparagus/parse_pgen_generated.py

./test.sh
