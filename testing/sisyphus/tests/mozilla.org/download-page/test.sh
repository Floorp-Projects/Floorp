#!/usr/local/bin/bash -e

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

TEST_DLDIR=${TEST_DLDIR:-$TEST_DIR/tests/mozilla.org/download-page}

TEST_DOWNLOAD_PAGE_TIMEOUT=${TEST_DOWNLOAD_PAGE_TIMEOUT:-60}
TEST_DOWNLOAD_BUILD_TIMEOUT=${TEST_DOWNLOAD_BUILD_TIMEOUT:-900}

#
# options processing
#
options="u:t:p:b:x:c:N:d:"
function usage()
{
    cat <<EOF
usage: 
$SCRIPT -u allurl -t test -p product -b branch -N profilename -x executablepath 
        -c credentials [-d datafiles]

variable            description
===============     ============================================================
-u allurl           required. location to find download "all" page.
-t test             required. all for download page. ftp for ftp directory
-p product          required. firefox|thunderbird
-b branch           required. 1.8.0|1.8.1|1.9.0
-x executablepath   required. directory-tree containing executable 'product'
-c credentials      optional. user:password if required for authentication
-N profilename      required. profile name 
-d datafiles        optional. one or more filenames of files containing 
                    environment variable definitions to be included.

                    note that the environment variables should have the same 
                    names as in the "variable" column.


allurl         =$allurl 
test           =$test 
product        =$product 
branch         =$branch 
profilename    =$profilename 
executablepath =$executablepath 
credentials    =$credentials 
datafiles      =$datafiles

EOF
    exit 2
}

unset allurl test product branch profilename executablepath credentials datafiles

while getopts $options optname ; 
  do 
  case $optname in
      u) allurl=$OPTARG;;
      t) test=$OPTARG;;
      p) product=$OPTARG;;
      b) branch=$OPTARG;;
      N) profilename=$OPTARG;;
      x) executablepath=$OPTARG;;
      c) credentials="-c $OPTARG";;
      d) datafiles=$OPTARG;;
  esac
done

# include environment variables
if [[ -n "$datafiles" ]]; then
    for datafile in $datafiles; do 
        source $datafile
    done
fi

if [[ -z "$allurl" || -z "$test" || \
    -z "$product" || -z "$branch" || -z "$profilename" || \
    -z "$executablepath" ]]; then
    usage
fi

executable=`get_executable $product $branch $executablepath`

urlfile=`mktemp /tmp/URLS.XXXX`

if [[ "$test" == "all" ]]; then
    $TEST_BIN/timed_run.py $TEST_DOWNLOAD_PAGE_TIMEOUT "test download" \
	"$executable" -P "$profilename" -spider -start -quit \
	-uri "$allurl" -timeout=$TEST_DOWNLOAD_PAGE_TIMEOUT \
	-hook "http://$TEST_HTTP/tests/mozilla.org/download-page/collect-urls-userhook.js" | \
	grep 'href: ' | sed 's/^href: //' > $urlfile
elif [[ "$test" == "ftp" ]]; then
    $TEST_BIN/timed_run.py $TEST_DOWNLOAD_PAGE_TIMEOUT "test download" \
	"$executable" -P "$profilename" -spider -start -quit \
	-uri "$allurl" -timeout=$TEST_DOWNLOAD_PAGE_TIMEOUT \
	-hook "http://$TEST_HTTP/tests/mozilla.org/download-page/userhook-ftp.js" | \
	grep 'href: ' | sed 's/^href: //' > $urlfile
fi

cat $urlfile | while read url; do

    echo "Processing $url"

    if [[ "$test" == "all" ]]; then
	downloadproduct=`echo $url | sed 's|.*product=\([^\-]*\).*|\1|'`
	downloadversion=`echo $url | sed 's|.*product=[^\-]*-\([0-9.]*\).*|\1|'`
    elif [[ "$test" == "ftp" ]]; then
	downloadproduct=`echo $url | sed 's|.*/\([a-zA-Z]*\)-[^/]*|\1|'`
	downloadversion=`echo $url | sed 's|\(.*\)/\([^/]*\)|\2|' | sed 's|[a-zA-Z]*-\([0-9.]*\)\..*|\1|'`
    fi

    echo "downloadproduct=$downloadproduct"
    echo "downloadversion=$downloadversion"

    case "$downloadversion" in
	1.5*)
	    downloadbranch="1.8.0"
	    ;;
	2.*)
	    downloadbranch="1.8.1"
	    ;;
	3.*)
	    downloadbranch="1.9.0"
	    ;;
	*)
	    echo "unknown download branch: $downloadbranch"
	    exit 2
	    ;;
    esac

    filepath=`mktemp /tmp/DOWNLOAD.XXXXXX`
    
    downloadexecutablepath="/tmp/download-$downloadproduct-$downloadbranch"
    downloadprofilepath="/tmp/download-$downloadproduct-$downloadbranch-profile"
    downloadprofilename="download-$downloadproduct-$downloadbranch-profile"

    if ! download.sh -u "$url" -f $filepath $credentials; then
	continue;
    fi

    if ! install-build.sh  -p "$downloadproduct" -b "$downloadbranch" \
	-x $downloadexecutablepath \
	-f $filepath; then
	continue
    fi

    rm $filepath

    if [[ "$downloadproduct" == "thunderbird" ]]; then
	template="-L $TEST_DIR/profiles/imap"
    else
	unset template
    fi

    if ! create-profile.sh -p "$downloadproduct" -b "$downloadbranch" \
	-x $downloadexecutablepath \
	-D $downloadprofilepath \
	-N $downloadprofilename \
	-U $TEST_DIR/prefs/mail-user.js \
	$template; then
	continue
    fi

    if ! install-extensions.sh -p "$downloadproduct" -b "$downloadbranch" \
	-x $downloadexecutablepath \
	-N $downloadprofilename \
	-E $TEST_DIR/xpi; then
	continue
    fi

    if ! check-spider.sh -p "$downloadproduct" -b "$downloadbranch" \
	-x $downloadexecutablepath \
	-N $downloadprofilename; then
	continue
    fi

    downloadexecutable=`get_executable $downloadproduct $downloadbranch $downloadexecutablepath`

    $TEST_BIN/timed_run.py $TEST_DOWNLOAD_BUILD_TIMEOUT "..." \
	"$downloadexecutable" \
	-spider -P $downloadprofilename \
	-uri "http://$TEST_HTTP/bin/buildinfo.html" \
	-hook "http://$TEST_HTTP/tests/mozilla.org/download-page/userhook.js" \
	-start -quit

    if ! uninstall-build.sh  -p "$downloadproduct" -b "$downloadbranch" \
	-x $downloadexecutablepath; then
	continue
    fi

done

rm $urlfile
