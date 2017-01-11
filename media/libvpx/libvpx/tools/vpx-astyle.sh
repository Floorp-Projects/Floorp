#!/bin/sh
set -e
astyle --style=java --indent=spaces=2 --indent-switches\
       --min-conditional-indent=0 \
       --pad-oper --pad-header --unpad-paren \
       --align-pointer=name \
       --indent-preprocessor --convert-tabs --indent-labels \
       --suffix=none --quiet --max-instatement-indent=80 "$@"
# Disabled, too greedy?
#sed -i 's;[[:space:]]\{1,\}\[;[;g' "$@"

sed_i() {
  # Incompatible sed parameter parsing.
  if sed -i 2>&1 | grep -q 'requires an argument'; then
    sed -i '' "$@"
  else
    sed -i "$@"
  fi
}

sed_i -e 's/[[:space:]]\{1,\}\([,;]\)/\1/g' \
      -e 's/[[:space:]]\{1,\}\([+-]\{2\};\)/\1/g' \
      -e 's/,[[:space:]]*}/}/g' \
      -e 's;//\([^/[:space:]].*$\);// \1;g' \
      -e 's/^\(public\|private\|protected\):$/ \1:/g' \
      -e 's/[[:space:]]\{1,\}$//g' \
      "$@"
