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

package Support::Files;

@testitems = split("\n",qq(
bug_form.pl #1
buglist.cgi #2
changepassword.cgi #3
checksetup.pl #4
colchange.cgi #5
collectstats.pl #6
createaccount.cgi #7
createattachment.cgi #8
defparams.pl #9
describecomponents.cgi #10
describekeywords.cgi #11
doeditparams.cgi #12
doeditvotes.cgi #13
duplicates.cgi #14
editcomponents.cgi #15
editgroups.cgi #16
editkeywords.cgi #27
editmilestones.cgi #18
editparams.cgi #19
editproducts.cgi #20
editusers.cgi #21
editversions.cgi #22
enter_bug.cgi #23
globals.pl #24
importxml.pl #25
long_list.cgi #26
move.pl #27
new_comment.cgi #28
post_bug.cgi #29
process_bug.cgi #30
query.cgi #31
queryhelp.cgi #32
quips.cgi #33
relogin.cgi #34
reports.cgi #35
sanitycheck.cgi #36
show_activity.cgi #37
show_bug.cgi #38
showattachment.cgi #39
showdependencygraph.cgi #40
showdependencytree.cgi #41
showvotes.cgi #42
syncshadowdb #43
token.cgi #44
userprefs.cgi #45
whineatnews.pl #46
xml.cgi #47
attachment.cgi #48
editattachstatuses.cgi #49
globals.pl #50
CGI.pl #51
));
