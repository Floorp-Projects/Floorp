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
# sh.exe instead of cmd.exe to process commands).
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

# Clean up the mess.
clean:
	rm -f *-gdf.png *~

