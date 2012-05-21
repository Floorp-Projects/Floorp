#! /bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#original author: Alice Nodelman
#contributor: Darin Fisher
#
#takes two inputs, $1 = file containing list of web pages of form http://pagename
#                  $2 = output file where list of index files is dumped - useful for places list of links into scripts
#
# web pages are dropped in directories named for their urls

if [ $# != 2 ]; then
	echo 'missing command line arguments'
	echo
	echo 'usage: getpages.sh inputfile outputfile'
	echo '	inputfile: file containing one url per line of the form http://url'
	echo '	outputfile: file to be created during execution, contains a list of index files one per url'
	exit 
fi

# generates the list of files to be cleansed (exclude image files)
# disables any existing call-outs to the live web
# provided by Darin Fisher
cleanse_files() {
	  find "$1" -type f -a -print0 ! -iname \*.jpg -a ! -iname \*.gif -a ! -iname \*.png -a ! -name \*.bak | xargs -0 perl -pi -e 's/[a-zA-Z0-9_]*.writeln/void/g;' -e 's/[a-zA-Z0-9_]*.write/void/g;' -e 's/[a-zA-Z0-9_]*.open/void/g;' -e 's/"https/"httpsdisabled/gi;' -e 's/"http/"httpdisabled/gi;' -e 's/<object/<objectdisabled/gi;' -e 's/<embed/<embeddisabled/gi;' -e 's/load/loaddisabled/g;'
}

mkdir testpages
cd testpages
for URL in $(cat ../$1); do
	#strip the leading http:// from the url
	CLEANURL=$(echo $URL | sed -e 's/http:\/\/\(.*\)/\1/')
	#create a directory with the cleaned url as the name
	echo "grabbing "$CLEANURL
	mkdir $CLEANURL
	cd $CLEANURL
	../../wget-1.10-css-parser/src/wget -p -k -H -E -erobots=off --no-check-certificate -U "Mozilla/5.0 (firefox)" --restrict-file-names=windows $URL -o outputlog.txt
	#figure out where/what the index file for this page is from the wget output log
	FILENAME=$(grep "saved" outputlog.txt | head -1 | sed -e "s/.*\`\(.*\)\'.*/\1/")
	rm outputlog.txt
	cd ..

	#do the final cleanup of any dangling urls
	#with thanks to Darin Fisher for the code
	cleanse_files $CLEANURL

	#add the index file link to the list of index files being generated
	echo $CLEANURL/$FILENAME >> $2
done
cd ..

