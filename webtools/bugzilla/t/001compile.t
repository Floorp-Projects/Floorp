# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code are the Bugzilla Tests.
# 
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are 
# Copyright (C) 2001 Zach Lipton.  All
# Rights Reserved.
# 
# Contributor(s): Zach Lipton <zach@zachlipton.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 


#################
#Bugzilla Test 1#
###Compilation###
BEGIN { use lib 't/'; }
BEGIN { use Support::Files; }
BEGIN { $tests = @Support::Files::testitems + 4; }
BEGIN { use Test::More tests => $tests; }

# First now we test the scripts                                                   
@testitems = @Support::Files::testitems; 

our $warnings;
my $verbose = $::ENV{VERBOSE};
$perlapp=$^X;
foreach $file (@testitems) {
        $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
        next if (!$file); # skip null entries
	$command = "$perlapp"." -c $file 2>&1";
	$loginfo=`$command`;
#	  print '@@'.$loginfo.'##';
	if ($loginfo =~ /syntax ok$/im) {
		$warnings{$_} = 1 foreach ($loginfo =~ /\((W.*?)\)/mg);
		if ($1) {
                        if ($verbose) { print STDERR $loginfo; }
                        ok(0,$file."--WARNING");
                } else {
			ok(1,$file);
		}
	} else {
                if ($verbose) { print STDERR $loginfo; }
		ok(0,$file."--ERROR");
        }
}      

# and the libs:                                                                 
use_ok('Token'); # 52                                                 
use_ok('Attachment'); # 53                                            
use_ok('Bug'); # 54                                            
use_ok('RelationSet'); # 55                                           



