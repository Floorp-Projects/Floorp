#!/bin/awk -f

BEGIN {
  n_exports = 0;
}

{
  if ($0 ~ /^exports.*=/) {
    exports[n_exports] = substr($3, 0, length($3) - 1);
    n_exports++;
  } else {
    print;
  }
}

END {
  for (i = 0; i < n_exports; i++) {
    print "this." exports[i] " = " exports[i] ";";
  }
}
