#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is used to plot simulation dynamics.
# Able to plot each flow separately. Other plot boxes can be added,
# currently one for Throughput, one for Latency and one for Packet Loss.

import matplotlib
import matplotlib.pyplot as plt
import numpy
import re
import sys

# Change this to True to save the figure to a file. Look below for details.
save_figure = False

class Variable(object):
  def __init__(self, variable):
    self._ID = variable[0]
    self._xlabel = variable[1]
    self._ylabel = variable[2]
    self._subplot = variable[3]
    self._y_max = variable[4]
    self.samples = dict()

  def getID(self):
    return self._ID

  def getXLabel(self):
    return self._xlabel

  def getYLabel(self):
    return self._ylabel

  def getSubplot(self):
    return self._subplot

  def getYMax(self):
    return self._y_max

  def getNumberOfFlows(self):
    return len(self.samples)


  def addSample(self, line):
    groups = re.search(r'_(((\d)+((,(\d)+)*))_(\D+))#\d@(\S+)', line)

    # Each variable will be plotted in a separated box.
    var_name = groups.group(1)
    alg_name = groups.group(8)

    alg_name = alg_name.replace('_', ' ')

    if alg_name not in self.samples.keys():
      self.samples[alg_name] = {}

    if var_name not in self.samples[alg_name].keys():
      self.samples[alg_name][var_name] = []

    sample = re.search(r'(\d+\.\d+)\t([-]?\d+\.\d+)', line)

    s = (sample.group(1), sample.group(2))
    self.samples[alg_name][var_name].append(s)

def plotVar(v, ax, show_legend, show_x_label):
  if show_x_label:
    ax.set_xlabel(v.getXLabel(), fontsize='large')
  ax.set_ylabel(v.getYLabel(), fontsize='large')

  for alg in v.samples.keys():

    for series in v.samples[alg].keys():

      x = [sample[0] for sample in v.samples[alg][series]]
      y = [sample[1] for sample in v.samples[alg][series]]
      x = numpy.array(x)
      y = numpy.array(y)

      line = plt.plot(x, y, label=alg, linewidth=4.0)

      colormap = {'Available0':'#AAAAAA',
                  'Available1':'#AAAAAA',
                  'GCC0':'#80D000',
                  'GCC1':'#008000',
                  'GCC2':'#00F000',
                  'GCC3':'#00B000',
                  'GCC4':'#70B020',
                  'NADA0':'#0000AA',
                  'NADA1':'#A0A0FF',
                  'NADA2':'#0000FF',
                  'NADA3':'#C0A0FF',
                  'NADA4':'#9060B0',}

      flow_id = re.search(r'(\d+(,\d+)*)', series)  # One or multiple ids.
      key = alg + flow_id.group(1)

      if key in colormap:
        plt.setp(line, color=colormap[key])
      elif alg == 'TCP':
        plt.setp(line, color='#AAAAAA')
      else:
        plt.setp(line, color='#654321')

      if alg.startswith('Available'):
        plt.setp(line, linestyle='--')
      plt.grid(True)

      # x1, x2, y1, y2
      _, x2, _, y2 = plt.axis()
      if v.getYMax() >= 0:
        y2 = v.getYMax()
      plt.axis((0, x2, 0, y2))

    if show_legend:
      plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.40),
                 shadow=True, fontsize='large', ncol=len(v.samples))

def main():
  variables = [
          ('Throughput_kbps', "Time (s)", "Throughput (kbps)", 1, 4000),
          ('Delay_ms', "Time (s)", "One-way Delay (ms)", 2, 500),
          ('Packet_Loss', "Time (s)", "Packet Loss Ratio", 3, 1.0),
          # ('Sending_Estimate_kbps', "Time (s)", "Sending Estimate (kbps)",
          #                                                        4, 4000),
          ]

  var = []

  # Create objects.
  for variable in variables:
    var.append(Variable(variable))

  # Add samples to the objects.
  for line in sys.stdin:
    if line.startswith("[ RUN      ]"):
      test_name = re.search(r'\.(\w+)', line).group(1)
    if line.startswith("PLOT"):
      for v in var:
        if v.getID() in line:
          v.addSample(line)

  matplotlib.rcParams.update({'font.size': 48/len(variables)})

  # Plot variables.
  fig = plt.figure()

  # Offest and threshold on the same plot.
  n = var[-1].getSubplot()
  i = 0
  for v in var:
    ax = fig.add_subplot(n, 1, v.getSubplot())
    plotVar(v, ax, i == 0, i == n - 1)
    i += 1

  if save_figure:
    fig.savefig(test_name + ".png")
  plt.show()

if __name__ == '__main__':
  main()
