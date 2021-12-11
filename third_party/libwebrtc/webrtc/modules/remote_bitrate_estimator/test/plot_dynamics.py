#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is used to plot simulation dynamics. The expected format is
# PLOT <plot_number> <var_name>:<ssrc>@<alg_name> <time> <value>
# <var_name> may optionally be followed by #<axis_alignment> but it is
# deprecated. <plot_number> is also deprecated.
# Each combination <var_name>:<ssrc>@<alg_name> is stored in it's own time
# series. The main function defines which time series should be displayed and
# whether they should should be displayed in the same or separate windows.


import matplotlib.pyplot as plt
import numpy
import re
import sys

# Change this to True to save the figure to a file. Look below for details.
SAVE_FIGURE = False

class ParsePlotLineException(Exception):
  def __init__(self, reason, line):
    super(ParsePlotLineException, self).__init__()
    self.reason = reason
    self.line = line


def ParsePlotLine(line):
  split_line = line.split()
  if len(split_line) != 5:
    raise ParsePlotLineException("Expected 5 arguments on line", line)
  (plot, _, annotated_var, time, value) = split_line
  if plot != "PLOT":
    raise ParsePlotLineException("Line does not begin with \"PLOT\\t\"", line)
  # The variable name can contain any non-whitespace character except "#:@"
  match = re.match(r'([^\s#:@]+)(?:#\d)?:(\d+)@(\S+)', annotated_var)

  if match == None:
    raise ParsePlotLineException("Could not parse variable name, ssrc and \
                                 algorithm name", annotated_var)
  var_name = match.group(1)
  ssrc = match.group(2)
  alg_name = match.group(3).replace('_', ' ')

  return (var_name, ssrc, alg_name, time, value)


def GenerateLabel(var_name, ssrc, ssrc_count, alg_name):
  label = var_name
  if ssrc_count > 1 or ssrc != "0":
    label = label + " flow " + ssrc
  if alg_name != "-":
    label = label + " " + alg_name
  return label


class Figure(object):
  def __init__(self, name):
    self.name = name
    self.subplots = []

  def AddSubplot(self, var_names, xlabel, ylabel):
    self.subplots.append(Subplot(var_names, xlabel, ylabel))

  def AddSample(self, var_name, ssrc, alg_name, time, value):
    for s in self.subplots:
      s.AddSample(var_name, ssrc, alg_name, time, value)

  def PlotFigure(self, fig):
    n = len(self.subplots)
    for i in range(n):
      axis = fig.add_subplot(n, 1, i+1)
      self.subplots[i].PlotSubplot(axis)

class Subplot(object):
  def __init__(self, var_names, xlabel, ylabel):
    self.xlabel = xlabel
    self.ylabel = ylabel
    self.var_names = var_names
    self.samples = dict()

  def AddSample(self, var_name, ssrc, alg_name, time, value):
    if var_name not in self.var_names:
      return

    if alg_name not in self.samples.keys():
      self.samples[alg_name] = {}
    if ssrc not in self.samples[alg_name].keys():
      self.samples[alg_name][ssrc] = {}
    if var_name not in self.samples[alg_name][ssrc].keys():
      self.samples[alg_name][ssrc][var_name] = []

    self.samples[alg_name][ssrc][var_name].append((time, value))

  def PlotSubplot(self, axis):
    axis.set_xlabel(self.xlabel)
    axis.set_ylabel(self.ylabel)

    count = 0
    for alg_name in self.samples.keys():
      for ssrc in self.samples[alg_name].keys():
        for var_name in self.samples[alg_name][ssrc].keys():
          x = [sample[0] for sample in self.samples[alg_name][ssrc][var_name]]
          y = [sample[1] for sample in self.samples[alg_name][ssrc][var_name]]
          x = numpy.array(x)
          y = numpy.array(y)
          ssrc_count = len(self.samples[alg_name].keys())
          l = GenerateLabel(var_name, ssrc, ssrc_count, alg_name)
          if l == 'MaxThroughput_':
            plt.plot(x, y, label=l, linewidth=4.0)
          else:
            plt.plot(x, y, label=l, linewidth=2.0)
          count += 1

    plt.grid(True)
    if count > 1:
      plt.legend(loc='best')


def main():
  receiver = Figure("PacketReceiver")
  receiver.AddSubplot(['Throughput_kbps', 'MaxThroughput_', 'Capacity_kbps',
                       'PerFlowCapacity_kbps', 'MetricRecorderThroughput_kbps'],
                      "Time (s)", "Throughput (kbps)")
  receiver.AddSubplot(['Delay_ms_', 'Delay_ms'], "Time (s)",
                      "One-way delay (ms)")
  receiver.AddSubplot(['Packet_Loss_'], "Time (s)", "Packet Loss Ratio")

  kalman_state = Figure("KalmanState")
  kalman_state.AddSubplot(['kc', 'km'], "Time (s)", "Kalman gain")
  kalman_state.AddSubplot(['slope_1/bps'], "Time (s)", "Slope")
  kalman_state.AddSubplot(['var_noise'], "Time (s)", "Var noise")

  detector_state = Figure("DetectorState")
  detector_state.AddSubplot(['T', 'threshold'], "Time (s)", "Offset")

  trendline_state = Figure("TrendlineState")
  trendline_state.AddSubplot(["accumulated_delay_ms", "smoothed_delay_ms"],
                             "Time (s)", "Delay (ms)")
  trendline_state.AddSubplot(["trendline_slope"], "Time (s)", "Slope")

  target_bitrate = Figure("TargetBitrate")
  target_bitrate.AddSubplot(['target_bitrate_bps', 'acknowledged_bitrate'],
                            "Time (s)", "Bitrate (bps)")

  min_rtt_state = Figure("MinRttState")
  min_rtt_state.AddSubplot(['MinRtt'], "Time (s)", "Time (ms)")

  # Select which figures to plot here.
  figures = [receiver, detector_state, trendline_state, target_bitrate,
    min_rtt_state]

  # Add samples to the figures.
  for line in sys.stdin:
    if line.startswith("[ RUN      ]"):
      test_name = re.search(r'\.(\w+)', line).group(1)
    if line.startswith("PLOT"):
      try:
        (var_name, ssrc, alg_name, time, value) = ParsePlotLine(line)
        for f in figures:
          # The sample will be ignored bv the figures that don't need it.
          f.AddSample(var_name, ssrc, alg_name, time, value)
      except ParsePlotLineException as e:
        print e.reason
        print e.line

  # Plot figures.
  for f in figures:
    fig = plt.figure(f.name)
    f.PlotFigure(fig)
    if SAVE_FIGURE:
      fig.savefig(test_name + f.name + ".png")
  plt.show()

if __name__ == '__main__':
  main()
