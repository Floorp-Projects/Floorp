#!/bin/tcsh
#sspitzer@netscape.com

# check args
if ($#argv != 1) then
	if ($#argv != 2) then
		echo  "Usage:  $0 <new-theme-name> [theme-to-copy]"
		echo  "\t<new-theme-name> is required"
		echo  "\t[theme-to-copy] is optional.  if none specified, uses modern"
		exit 1
	endif
endif

# check if it exists
test -d $1
if ($? == 0) then
	echo "$1 already exists"
	exit 1
endif

if ($#argv == 1) then
	# copy modern skin
	\cp -r modern $1
else
	# make sure theme-to-copy exists
	test -d $2
	if ($?) then 
		echo "$2 does not exist"
		exit 1
	endif
	\cp -r $2 $1
endif

# remove the CVS files
(cd $1; find . -name "CVS" -exec rm -rf {} \; >& /dev/null)

# create a theme.mk
echo "THEME=$1" > $1/theme.mk

# create a new README
echo "this is the $1 theme" > $1/README
date >> $1/README

# notify the user
echo "$1 theme has been created.  now you have to check it in."
