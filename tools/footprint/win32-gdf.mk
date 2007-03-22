# -*- Mode: Makefile -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Makefile, released
# November 13, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Waterson <waterson@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This makefile takes raw data files named ``winEmbed.dat'' and
# ``mozilla.dat'' produces a graph that shows memory usage and peak
# memory usage versus number of URLs loaded.
#
# The data files are assumed to be of the form:
#
#   <working-set-size-1> <peak-working-set-size-1>
#   <working-set-size-2> <peak-working-set-size-2>
#   ...
#
# It is also assumed that each measurement corresponds (roughly) to a
# URL load.
#
# You can tweak ``win32.gnuplot.in'' to change the graph's output.
#
# You should use this with ``make --unix'' (which will use
# sh.exe instead of cmd.exe to process commands); e.g.,
#
#   make --unix -f win32-gdf.mk \
#     BUSTER_URL="http://localhost/cgi-bin/buster.cgi?refresh=10"
#
# What You'll Need
# ----------------
#
# . Get gnuplot for Win32 from
#
#     ftp://ftp.dartmouth.edu/pub/gnuplot/gnuplot3.7cyg.zip
#
# . The "standard" cygwin tools that you probably already have. (If
#   you don't have 'em, see the Win32 build instructions on
#   mozilla.org.)
#

# This script computes a line using linear regression; it's output is
# of the form:
#
#   <b1> * x + <b0>
#
# Where <b1> is the slope and <b0> is the y-intercept.
LINEAR_REGRESSION=awk -f linear-regression.awk

PROGRAM_PATH=..\\..\\dist\\win32_o.obj\\bin
WINEMBED_PROGRAM=winEmbed
MOZILLA_PROGRAM=mozilla

GNUPLOT=wgnuplot.exe
BUSTER_URL=http://btek/cgi-bin/buster.cgi?refresh=10

#----------------------------------------------------------------------
# Top-level target
#
all: win32-gdf.png

#----------------------------------------------------------------------
# winEmbed
#

.INTERMEDIATE: winEmbed-ws.dat winEmbed-pws.dat mozilla-ws.dat mozilla-pws.dat win32.gnuplot

# Create a PNG image using the generated ``win32.gnuplot'' script
win32-gdf.png: winEmbed-ws.dat winEmbed-pws.dat mozilla-ws.dat mozilla-pws.dat win32.gnuplot
	$(GNUPLOT) win32.gnuplot

# Generate a ``gnuplot'' script from ``win32.gnuplot.in'', making
# appropriate substitutions as necessary.
win32.gnuplot: win32.gnuplot.in winEmbed-ws.dat mozilla-ws.dat
	sed -e "s/@WINEMBED-WS-LINE@/`$(LINEAR_REGRESSION) winEmbed-ws.dat`/" \
	    -e "s/@WINEMBED-GROWTH-RATE@/`$(LINEAR_REGRESSION) winEmbed-ws.dat | awk '{ printf \"%0.1f\n\", $$1; }'`/" \
	    -e "s/@WINEMBED-BASE-SIZE@/`$(LINEAR_REGRESSION) winEmbed-ws.dat | awk '{ print $$5; }'`/" \
	    -e "s/@MOZILLA-WS-LINE@/`$(LINEAR_REGRESSION) mozilla-ws.dat`/" \
	    -e "s/@MOZILLA-GROWTH-RATE@/`$(LINEAR_REGRESSION) mozilla-ws.dat | awk '{ printf \"%0.1f\n\", $$1; }'`/" \
	    -e "s/@MOZILLA-BASE-SIZE@/`$(LINEAR_REGRESSION) mozilla-ws.dat | awk '{ print $$5; }'`/" \
		win32.gnuplot.in > $@

# Break the raw data file into temporary files that can be processed
# by gnuplot directly.
winEmbed-ws.dat: winEmbed.dat
	awk '{ print NR, $$1 / 1024; }' $? > $@

winEmbed-pws.dat: winEmbed.dat
	awk '{ print NR, $$2 / 1024; }' $? > $@

mozilla-ws.dat: mozilla.dat
	awk '{ print NR, $$1 / 1024; }' $? > $@

mozilla-pws.dat: mozilla.dat	
	awk '{ print NR, $$2 / 1024; }' $? > $@

# Run programs to collect data
winEmbed.dat: wm.exe
	cmd /c "start $(PROGRAM_PATH)\\$(WINEMBED_PROGRAM) $(BUSTER_URL) && .\\wm $(WINEMBED_PROGRAM) > $@"

mozilla.dat: wm.exe
	cmd /c "start $(PROGRAM_PATH)\\$(MOZILLA_PROGRAM) $(BUSTER_URL) && .\\wm $(MOZILLA_PROGRAM) > $@"

# Build ``wm.exe'', the memory spy
wm.exe: wm.cpp
	cl -Od -Zi wm.cpp advapi32.lib

# Clean up the mess.
clean:
	rm -f wm.exe *-gdf.png *.dat *~

