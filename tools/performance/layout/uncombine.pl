##########################################################################################
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# uncombine.pl : Break up the combined performance run output file into a file per URL
#
# When viewer or mozilla are run and provided a command line argument of '-f urlfile.txt'
# then the file 'urlfile.txt' is read and each line is expected to be a URL. The browser 
# then cycles through each url and loads it, waiting for the load to end or a safety 
# timeout to occur. In eaiter case the next url is loaded until there are no more and then
# the browser exits. The output from the run (stdout) is captured to a file which then
# contains all of the performance output we need to build charts of performance numbers
# (see average2.pl, along with header.pl and footer.pl).
#
# ASSUMES file urls are pathed as: file:///S|/Mozilla/Tools/ProfTools/WebSites/URL/FILE
#         or if an http URL: http://URL/FILE
#  Normally it is expected that local files will be used, however if we should use http
#  URLs they can be processed (have not tested to date...)
#
#  For file urls, the websites tree is assumed to be the one committed to cvs. Installed
#  files will be at s:\mozilla\tools\perftools\websites\url\file
#  If you have the files in a different location, adjust the part that extracts the url
#  (most likely the index into the tokens will change. Search for SENSITIVE in the script)

#------------------------------------------------------------------------------
sub debug_print {
  foreach $str (@_){
#    print( $str );
  }
}
#------------------------------------------------------------------------------


@ARGV;
$dir="Logs\\";
$i=0;
$TimingBlockBegun=0;
$fileURL=0;
$httpURL=0;
$shellID;
$infileName = $ARGV[0];
$supportHTTP = 0; # set to 1 if HTTP URLs are to be supported

open(COMBINEFILE, "< $infileName") or die "Unable to open $infileName\n";
while(<COMBINEFILE>){
	$url;
	@tokens = split( / /, $_ );
	
	if($TimingBlockBegun == 0) {
		# look for the start of a new block
		if($_ =~ /Timing layout processes on url/){
			debug_print( "Timing begin candidate: $_ \n" );

			# see if it is a file or http url. 
			# If so, we are starting, otherwise it is probably a chrome url so ignore it
			if( $_ =~ /url: \'file:/ ){
				debug_print( " - file URL\n" );
				$url = $tokens[6];
				$TimingBlockBegun=1;
				$httpURL=0;
				$fileURL=1;
			}
      if($supportHTTP > 0) {
			  if( $_ =~ /url: \'http:/  ){
				  debug_print( "http URL\n" );
				  $url = $tokens[6];			### SENSITIVE to installation path
				  $TimingBlockBegun=1;
				  $fileURL=0;
				  $httpURL=1;
			  }
      }

			# if we got a valid block then extract the WebShellID 
			# for matching the end-of-block later
			if($TimingBlockBegun > 0){
				chop($url);
				$shellID = $tokens[8];
				chop( $shellID );
				debug_print( " - WebShellID: $shellID\n");
				@urlParts = split(/\//, $url);
				if($fileURL > 0){
					$urlName = $urlParts[9];	### SENSITIVE to installation path
                                    ### eg. 'file:///S|/Mozilla/Tools/performance/layout/WebSites/amazon/index.html'
				} else {
					$urlName = $urlParts[2];	### http://all.of.this.is.the.url.name/index.html
				}
				open(URLFILE, ">$dir$urlName-log"."\.txt") or die "cannot open file $dir$urlName\n";
				print("Breaking out url $url into "."$dir$urlName-log"."\.txt"."\n");
			}
		}
	}

	if($TimingBlockBegun > 0){
    $done=0;
    $keepLine=1;
		# Look for end of block:
		#  - Find the line with the "Layout + Page Load" in it...
		if( $_ =~ /Layout \+ Page Load/ ){
			# Match the WebShell ID - if it is a match then our block ended,
			# otherwise it is the end of another block within our block
			$webshellID = "\(webBrowserChrome=".$shellID."\)";
			if( $tokens[6] =~ /$webshellID/ ){
				debug_print( "- WebShellID MATCH: $webshellID $tokens[6]\n" );
        $done=1;
			} else {
        $keepLine=0; 
      }
		}
    if($keepLine == 1){
		  # write the line to the file
		  print(URLFILE $_);
    }
    if($done == 1){
 			$TimingBlockBegun=0;
			close(URLFILE);
    }
	}
	$i++;
}
