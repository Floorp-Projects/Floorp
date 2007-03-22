##########################################################################################
#
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
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   2/10/00 attinasi
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
