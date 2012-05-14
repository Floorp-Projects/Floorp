#!/bin/sh

# Edit the doc Makefile to just echo the commands
sed -e 's/latex2man/echo/' -e 's/pdflatex/echo/' -i.old "$1"
