##########################################################################################
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape 
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 2/10/00 attinasi
#
##########################################################################################

# 1) Load Mozilla with an argument indicating where to find the URLs to cycle through
# 2) Uncombine the logfile into a log for each site
# 3) Run the script genfromlogs.pl to process the logs and generate the chart
# 4) Rename the chart to the ID provided
# 5) Run the script history.pl to generate the history chart
# 6) Move the files to a directory just for them...


# Get and check the arguments
@ARGV;
$ID = $ARGV[0];
$bldRoot = $ARGV[1];
$profID = $ARGV[2];
$arg=0;
$cmdLineArg[$arg++] = "-f ";
$cmdLineArg[$arg++] = "S:\\mozilla\\tools\\performance\\layout\\40-url.txt ";
#$cmdLineArg[$arg++] = "-ftimeout ";
#$cmdLineArg[$arg++] = "15 ";
#$cmdLineArg[$arg++] = "-P ";
#$cmdLineArg[$arg++] = "$profID ";

if(!$ID || !$bldRoot || !$profID){
  die "ID, Build Root and Profile Name must be provided. \n - Example: 'perl perf.pl Daily_021400 s:\\moz\\daily\\0214 Attinasi' \n";
}

# build up a full path and argument strings for the invocation
$mozPath;
$logName;
#$mozPath=$bldRoot."\\bin\\Mozilla.exe";
$mozPath=$bldRoot."\\bin\\Viewer.exe";
$logName="Logs\\".$ID."-combined.dat";

-e $mozPath or die "$mozPath could not be found\n";

# run it
$commandLine = $mozPath." ".$cmdLineArg[0].$cmdLineArg[1].$cmdLineArg[2].$cmdLineArg[3].$cmdLineArg[4].$cmdLineArg[5]."> $logName";
print("Running $commandLine\n");
system ("$commandLine") == 0 or die "Cannot invoke Mozilla\n";

# uncombine the output file
print("Breaking result into log files...\n");
system( ("perl", "uncombine.pl", "$logName") ) == 0 or die "Error uncombining the output\n";

# generate the chart from the logs
print("Generating performance table...\n");
system( ("perl", "genfromlogs.pl", "$ID", "$bldRoot") ) == 0 or die "Error generating the chart from the logs\n";

# rename the output
system( ("copy", "table.html", "Tables\\$ID\.html") ) == 0 or die "Error copying table.html to Tables\\$ID\.html\n";

# run the history script
print("Generating history table...\n");
system( ("perl", "history.pl") ) == 0 or die "Could not execute the history.pl script\n";

# rename the output
system( ("copy", "TrendTable.html", "Tables\\$ID-TrendTable.html") ) == 0 or die "could not copy TrendTable.html to Tables\\$ID-TrendTable.html\n";

# save off the files
system( ("mkdir", "Logs\\$ID") );
system( ("copy", "Logs\\*.*", "Logs\\$ID\\*.*") ) == 0 or die "Cannot copy logfiles to Logs\\$ID\\*.*\n";
system( ("copy", "history.txt", "Logs\\$ID_history.txt") ) == 0 or die "Cannot copy $ID_history.txt to Logs\n";

print("perf.pl DONE!\n");
