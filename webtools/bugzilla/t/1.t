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

BEGIN { use Test::More tests => 51; }
BEGIN { use lib '../'; }

# First, we test the libs

#require_ok('CGI.pl');
#require_ok('globals.pl');
BEGIN { use_ok('Token'); }
BEGIN { use_ok('RelationSet'); }
BEGIN { use_ok('Bug'); }

# and now we test the scripts
@testitems = split("\n",qq(
bug_form.pl #4
buglist.cgi #5
changepassword.cgi #6
checksetup.pl #7
colchange.cgi #8
collectstats.pl #9
createaccount.cgi #10
createattachment.cgi #11
defparams.pl #12
describecomponents.cgi #13
describekeywords.cgi #14
doeditparams.cgi #15
doeditvotes.cgi #16
duplicates.cgi #17
editcomponents.cgi #18
editgroups.cgi #19
editkeywords.cgi #20
editmilestones.cgi #21
editparams.cgi #22
editproducts.cgi #23
editusers.cgi #24
editversions.cgi #25
enter_bug.cgi #26
globals.pl #27
importxml.pl #28
long_list.cgi #29
move.pl #30
new_comment.cgi #31
post_bug.cgi #32
process_bug.cgi #33
query.cgi #34
queryhelp.cgi #35
quips.cgi #36
RelationSet.pm #37
relogin.cgi #38
reports.cgi #39
sanitycheck.cgi #40
show_activity.cgi #41
show_bug.cgi #42
showattachment.cgi #43
showdependencygraph.cgi #44
showdependencytree.cgi #45
showvotes.cgi #46
syncshadowdb #47
token.cgi #48
userprefs.cgi #49
whineatnews.pl #50
xml.cgi #51
));
our $warnings;
my $verbose = $::ENV{VERBOSE};
$perlapp='/usr/bonsaitools/bin/perl';
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
