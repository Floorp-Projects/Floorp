#!/bin/bash

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# To set up in e.g. Eclipse, run a separate shell and pipe the output from the
# test into this script.
#
# In Eclipse, that amounts to creating a Run Configuration which starts
# "/bin/bash" with the arguments "-c [trunk_path]/out/Debug/modules_unittests
# --gtest_filter=*BweTest* | [trunk_path]/webrtc/modules/
# remote_bitrate_estimator/bwe_plot.

# bwe_plot.sh has a single y axis and a dual y axis mode. If any line specifies
# a an axis by ending with "#<axis number (1 or 2)>" two y axis will be used,
# the first will be assumed to represent bitrate (in kbps) and the second will
# be assumed to represent time deltas (in ms).

log=$(</dev/stdin)

function gen_gnuplot_input {
  colors=(a7001f 0a60c2 b2582b 21a66c d6604d 4393c3 f4a582 92c5de edcbb7 b1c5d0)
  data_sets=$(echo "$log" | grep "^PLOT" | cut -f 2 | sort | uniq)
  linetypes=($(echo "$data_sets" | cut -d '#' -f 2 | cut -d ' ' -f 1))
  echo -n "reset; "
  echo -n "set terminal wxt size 1440,900 font \"Arial,9\"; "
  echo -n "set xlabel \"Seconds\"; "
  if [ -n $linetypes ]; then
    echo -n "set ylabel 'bitrate (kbps)';"
    echo -n "set ytics nomirror;"
    echo -n "set y2label 'time delta (ms)';"
    echo -n "set y2tics nomirror;"
  fi
  echo -n "plot "
  i=0
  for set in $data_sets ; do
    (( i++ )) && echo -n ","
    echo -n "'-' with "
    echo -n "linespoints "
    echo -n "ps 0.5 "
    echo -n "lc rgbcolor \"#${colors[$(($i % 10))]}\" "
    if [ -n ${linetypes[$i - 1]} ]; then
      echo -n "axes x1y${linetypes[$i - 1]} "
    elif [ -n $linestypes ]; then
      # If no line type is specified, but line types are used, we will default
      # to the bitrate axis.
      echo -n "axes x1y1 "
    fi
    echo -n "title \"$set\" "
  done
  echo
  for set in $data_sets ; do
    echo "$log" | grep "^PLOT.$set" | cut -f 3,4
    echo "e"
  done
}

gen_gnuplot_input "$log" | gnuplot -persist
