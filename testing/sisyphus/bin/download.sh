#!/bin/bash -e
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005.
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Bob Clary <bob@bclary.com>
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

args=$@
script=`basename $0`

#
# options processing
#
options="u:c:f:t:d:"
function usage()
{
    cat <<EOF
$SCRIPT $args

usage: 
$SCRIPT -u url [-c credentials] -f filepath [-t timeout] [-d datafiles]

variable            description
===============     ============================================================
-u url              required. url to download build from
-c credentials      optional. username:password
-f filepath         required. path to filename to store downloaded file
-t timeout          optional. timeout in seconds before download fails.
                    default 300 seconds
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

note that the environment variables should have the same names as in the 
"variable" column.

downloads file from url with optional authentication credentials
saving the file to filepath. If the path to the file does not exist,
it is created. If the download takes longer than timeout seconds,
the download is cancelled.

EOF
    exit 1
}

unset url credentials filepath timeout datafiles

while getopts $options optname ; 
  do 
  case $optname in
      u) url=$OPTARG;;
      c) credentials=$OPTARG;;
      f) filepath=$OPTARG;;
      t) timeout=$OPTARG;;
      d) datafiles=$OPTARG;;
  esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        cat $datafile | sed 's|^|data: |'
        source $datafile
    done
fi

if [[ -z $url || -z $filepath ]]
    then
    usage
fi

if [[ -n "$credentials" ]]; then
    auth="--user $credentials"
fi

timeout=${timeout:-300}


path=`dirname $filepath`

if [[ -z "$path" ]]; then
    echo "$SCRIPT: ERROR filepath path is empty"
    exit 2
fi

echo "url=$url filepath=$filepath credentials=$credentials timeout=$timeout"

# curl options
# -S show error if failure
# -s silent mode
# -L follow 3XX redirections
# -m $timeout time out 
# -D - dump headers to stdout
# --create-dirs create path if needed

if ! curl -LsS -m $timeout "$url" -D - --create-dirs -o $filepath $auth; then
    echo "$SCRIPT: FAIL Unable to download $url"
    exit 2
fi


