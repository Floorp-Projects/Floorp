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

@ARGV;
$milestone = $ARGV[0];
print ($milestone);

$bldRoot = $ARGV[1];
print(" BuildRoot: $bldRoot\n");
$cnt = 0;

# Backup the history file
system( ("copy", "history.txt", "history.bak" ) );

# Delete the average.txt file so we don;t get old values in the averages
if (-e "average.txt")
{
	system( ("del", "average.txt") );
}

# Run the Header script
system( ("perl", "header.pl", "$bldRoot", "$milestone", "$ARGV[2]" ) );

#
# now run the average2 script for each file in the logs directory
#
while( <Logs\\*.txt> ){
  $line = $_;
  $cnt++;
  if ($line =~ /-log.txt/ ){
    print( "File $cnt: $line\t" );
    @nameParts = split( /-/, $line );
    @nameNoDir = split( /\\/, $nameParts[0] );
    print( "Name: $nameNoDir[1]\n" );
    if ($nameNoDir[1] eq "") {
      print ("Skipping $line\n");
    } else {
      system( ("perl", "Averagetable2.pl", "$nameNoDir[1]", "$line", "$cnt", "$bldRoot", "$nameNoDir[1]", "$ARGV[2]" ) );
    }
  } else {
    print ("Skipping file $line\n");
  }
}

# Run the Footer script
system( ("perl", "footer.pl", "$bldRoot", "$milestone" ) );

print("Processed $cnt logs\n");
