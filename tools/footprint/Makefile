# -*- Mode: Makefile -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Makefile, released November 13, 2000.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    Chris Waterson <waterson@netscape.com>
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the MPL or the GPL.
#
#
#
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
#   make MOZILLA_DIR=/export2/waterson/seamonkey-opt/mozilla/dist/bin \
#     PROGRAM=gtkEmbed \
#     URL="http://localhost/cgi-bin/buster.cgi?refresh=10"
#
# To use this program, you'll need to:
#
# 1. Install gnuplot, e.g., using your RedHat distro.
# 2. Install the "buster.cgi" script onto a webserver somewhere
# 3. Have a mozilla build.
#
# You can tweak ``linux.gnuplot.in'' to change the graph's output.

# This script computes a line using linear regression; it's output is
# of the form:
#
#   <b1> * x + <b0>
#
# Where <b1> is the slope and <b0> is the y-intercept.
LINEAR_REGRESSION=awk -f linear-regression.awk

WATCH=watch.sh

MOZILLA_DIR=../../dist/bin
PROGRAM=mozilla-bin
BUSTER_URL=http://localhost/cgi-bin/buster.cgi?refresh=10

#----------------------------------------------------------------------
# Top-level target
#
all: gdf.png

#----------------------------------------------------------------------
# gtkEmbed
#

.INTERMEDIATE: linux.gnuplot vms.dat vmd.dat vml.dat rss.dat

# Create a PNG image using the generated ``linux.gnuplot'' script
gdf.png: vms.dat vmd.dat vml.dat rss.dat linux.gnuplot
	gnuplot linux.gnuplot

# Generate a ``gnuplot'' script from ``linux.gnuplot.in'', making
# appropriate substitutions as necessary.
linux.gnuplot: linux.gnuplot.in vms.dat
	sed -e "s/@PROGRAM@/$(PROGRAM)/" \
            -e "s/@VMS-LINE@/`$(LINEAR_REGRESSION) vms.dat`/" \
	    -e "s/@GROWTH-RATE@/`$(LINEAR_REGRESSION) vms.dat | awk '{ printf \"%0.1lf\\n\", $$1; }'`/" \
	    -e "s/@BASE-SIZE@/`$(LINEAR_REGRESSION) vms.dat | awk '{ print $$5; }'`/" \
		linux.gnuplot.in > linux.gnuplot

# Break the raw data file into temporary files that can be processed
# by gnuplot directly.
vms.dat: linux.dat
	awk '{ print NR, $$1; }' $? > $@

vmd.dat: linux.dat
	awk '{ print NR, $$2; }' $? > $@

vml.dat: linux.dat
	awk '{ print NR, $$3; }' $? > $@

rss.dat: linux.dat
	awk '{ print NR, $$4; }' $? > $@

# Run $(PROGRAM) to produce linux.dat
linux.dat:
	LD_LIBRARY_PATH=$(MOZILLA_DIR) \
	MOZILLA_FIVE_HOME=$(MOZILLA_DIR) \
	$(WATCH) -o $@ $(MOZILLA_DIR)/$(PROGRAM) "$(BUSTER_URL)"

# Clean up the mess.
clean:
	rm -f linux.dat gdf.png *~

