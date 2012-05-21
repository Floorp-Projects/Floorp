##########################################################################################
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
