#!/bin/bash
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
# The Original Code is Mozilla code.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Chris Pearce <chris@pearce.org.nz>
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

# YASM to MASM syntax translator plus assembler wrapper script.
# Takes standard gcc-style arguments, and calls yasm2masm.py to 
# translate the input source file, and then assembles the translated
# file using MASM.

output_file=
input_file=
translate_only=false
includes=
translate_to_masm=true

# Get the path of the script. We assume that yasm2masm.py is in the
# same directory as the script.
path=`dirname $0`

while [ $# -gt 0 ]
do
  case $1
  in
    -o)
      if ( test $# -gt 1 ) && ($output_file)
      then
        output_file=$2
        shift 2
      else
        echo "Supply one output file name with -o argument."
        exit 1
      fi
    ;;
    -dont-translate)
      translate_to_masm=false
      shift
    ;;
    -t)
      translate_only=true
      shift 1
    ;;
    -I)
      includes="$includes -I$2"
      shift 2
    ;;
    -c)
      # ignore -c
      shift 1
    ;;
    -I*)
      includes="$includes $1"
      shift 1
    ;;
    *)
      if [ $input_file ]
      then
        echo "Don't supply two input filenames!"
        exit 1
      fi
      input_file=$1
      shift 1
    ;;
  esac
done

translated_file=${output_file}".p"

if ( test -z $input_file )  || (test -z $output_file)
then
  echo "YASM-to-MASM translator-compiler wrapper."
  echo "Usage:"
  echo "   yasm2masm-as.sh [-t] -o output_file [-Iinclude1 -I include2] input_file"
  echo "   -t only runs the translator, storing result in \$output_file.p, does not assemble using MASM."
  exit 1
fi

# First run the translator
if [ $translate_to_masm == false ]
then
  translated_file=$input_file
else
  python ${path}/yasm2masm.py < $input_file > $translated_file
  if [ ! $? ]
  then
    echo "Translation from YASM to MASM syntax failed!"
    exit $?
  fi
fi
  
if [ $translate_only == true ]
then
  echo "Translated file is in $translated_file"
  exit
fi

# Assemble the translated file.
echo "YASM-to-MASM translate-compile: $input_file"
echo "ml -c -Fo $output_file $includes -D __OUTPUT_FORMAT__=win32 -safeseh $translated_file"
ml -c -Fo $output_file $includes -D __OUTPUT_FORMAT__=win32 -safeseh $translated_file

retval=$?

if [ $translate_to_masm == true ]
then
  rm ${translated_file}
fi

exit $retval

