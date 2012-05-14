#!/usr/bin/ksh -p
# 
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#ident	"$Id: bld_awk_pkginfo.ksh,v 1.4 2012/03/06 13:13:40 gerv%gerv.net Exp $"
#
# Simple script which builds the awk_pkginfo awk script.  This awk script
# is used to convert the pkginfo.tmpl files into pkginfo files
# for the build.
#

usage()
{
   cat <<-EOF
usage: bld_awk_pkginfo -p <prodver> -m <mach> -o <awk_script> [-v <version>]
EOF
}

#
# Awk strings
#
# two VERSION patterns: one for Dewey decimal, one for Dewey plus ,REV=n
# the first has one '=' the second has two or more '='
#
VERSION1="VERSION=[^=]*$"
VERSION2="VERSION=[^=]*=.*$"
PRODVERS="^SUNW_PRODVERS="
ARCH='ARCH=\"ISA\"'

#
# parse command line
#
mach=""
prodver=""
awk_script=""
version="NSPRVERS"

while getopts o:p:m:v: c
do
   case $c in
   o)
      awk_script=$OPTARG
      ;;
   m)
      mach=$OPTARG
      ;;
   p)
      prodver=$OPTARG
      ;;
   v)
      version=$OPTARG
      ;;
   \?)
      usage
      exit 1
      ;;
   esac
done

if [[ ( -z $prodver ) || ( -z $mach ) || ( -z $awk_script ) ]]
then
   usage
   exit 1
fi

if [[ -f $awk_script ]]
then
	rm -f $awk_script
fi

#
# Build REV= field based on date
#
rev=$(date "+%Y.%m.%d.%H.%M")

#
# Build awk script which will process all the
# pkginfo.tmpl files.
#
# the first VERSION pattern is replaced with a leading quotation mark
#
rm -f $awk_script
cat << EOF > $awk_script
/$VERSION1/ {
      sub(/\=[^=]*$/,"=\"$rev\"")
      print
      next
   }
/$VERSION2/ {
      sub(/\=[^=]*$/,"=$rev\"")
      sub(/NSPRVERS/,"$version")
      print
      next
   }
/$PRODVERS/ { 
      printf "SUNW_PRODVERS=\"%s\"\n", "$prodver" 
      next
   }
/$ARCH/ {
      printf "ARCH=\"%s\"\n", "$mach"
      next
   }
{ print }
EOF
