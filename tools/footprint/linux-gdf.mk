# -*- Mode: Makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This makefile will run Mozilla (or the program you specify), observe
# the program's memory status using the /proc filesystem, and generate
# a ``gross dynamic footprint'' graph using gnuplot.
#
# Usage:
#
#   make MOZILLA_DIR=<mozilla-dir> PROGRAM=<program> URL=<url>
#
# e.g.,
#
#   make -flinux-gdf.mk \
#     MOZILLA_DIR=/export2/waterson/seamonkey-opt/mozilla/dist/bin \
#     PROGRAM=gtkEmbed \
#     BUSTER_URL="http://localhost/cgi-bin/buster.cgi?refresh=10"
#
# To use this program, you'll need to:
#
# 1. Install gnuplot, e.g., using your RedHat distro.
# 2. Install the "buster.cgi" script onto a webserver somewhere
# 3. Have a mozilla build.
#
# You can tweak ``linux.gnuplot.in'' to change the graph's output.

# This script computes a line using linear regression; its output is
# of the form:
#
#   <b1> * x + <b0>
#
# Where <b1> is the slope and <b0> is the y-intercept.
LINEAR_REGRESSION=awk -f linear-regression.awk Skip=5

INTERVAL=10
WATCH=./watch.sh

MOZILLA_DIR=../../dist/bin
PROGRAM=gtkEmbed
BUSTER_URL=http://localhost/cgi-bin/buster.cgi?refresh=$(INTERVAL)
OUTFILE=linux.dat

#----------------------------------------------------------------------
# Top-level target
#
all: gdf.png

#----------------------------------------------------------------------
# gtkEmbed
#

.INTERMEDIATE: linux.gnuplot vms.dat vmd.dat vmx.dat rss.dat

# Create a PNG image using the generated ``linux.gnuplot'' script
gdf.png: vms.dat vmd.dat vmx.dat rss.dat linux.gnuplot
	gnuplot linux.gnuplot

# Generate a ``gnuplot'' script from ``linux.gnuplot.in'', making
# appropriate substitutions as necessary.
linux.gnuplot: linux.gnuplot.in vms.dat
	sed -e "s/@PROGRAM@/$(PROGRAM)/" \
            -e "s/@VMS-LINE@/`$(LINEAR_REGRESSION) vms.dat`/" \
	    -e "s/@GROWTH-RATE@/`$(LINEAR_REGRESSION) vms.dat | awk '{ printf \"%0.1lf\\n\", $$1; }'`/" \
	    -e "s/@BASE-SIZE@/`$(LINEAR_REGRESSION) vms.dat | awk '{ print $$5 + 2000; }'`/" \
		linux.gnuplot.in > linux.gnuplot

# Break the raw data file into temporary files that can be processed
# by gnuplot directly.
vms.dat: $(OUTFILE)
	awk -f create_dat.awk TYPE=vms $? > $@

vmd.dat: $(OUTFILE)
	awk -f create_dat.awk TYPE=vmd $? > $@

vmx.dat: $(OUTFILE)
	awk -f create_dat.awk TYPE=vmx $? > $@

rss.dat: $(OUTFILE)
	awk -f create_dat.awk TYPE=rss $? > $@

# Run $(PROGRAM) to produce $(OUTFILE)
$(OUTFILE):
	LD_LIBRARY_PATH=$(MOZILLA_DIR) \
	MOZILLA_FIVE_HOME=$(MOZILLA_DIR) \
	$(WATCH) -i $(INTERVAL) -o $@ $(MOZILLA_DIR)/$(PROGRAM) "$(BUSTER_URL)"

# Clean up the mess.
clean:
	rm -f $(OUTFILE) gdf.png *~

