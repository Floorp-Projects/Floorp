#!/usr/bonsaitools/bin/mysqltcl
# -*- Mode: tcl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

source CGI.tcl

Lock
LoadCheckins
LoadMOTD
LoadWhiteboard
LoadTreeConfig
Unlock


if {$treeopen} {
    set openword Open
} else {
    set openword Closed
}

puts "Content-type: text/html
Refresh: 300

<HTML>
<TITLE>($openword) Bonsai -- the art of effectively controlling trees</TITLE>

<IMG ALIGN=right SRC=bonsai.gif>
<H1>Bonsai -- Tree Control</H1>

<FORM name=treeform>
<H3>
<SELECT name=treeid size=1 onchange='submit();'>
"

# <SELECT name=treeid size=1 onchange='window.location =\"toplevel.cgi?treeid=\" + treeform.treeid.value;'>

foreach t $treelist {
    if {![info exists treeinfo($t,nobonsai)]} {
        if {[cequal $t $treeid]} {
            set c "SELECTED"
        } else {
            set c ""
        }
        puts "<OPTION VALUE=$t $c>$treeinfo($t,description)"
    }
}

puts "</SELECT></H3></FORM>"



if {$readonly} {
    puts "<h2><font color=red>Be aware that you are looking at an old hook!</font></h2>"
}

puts "<tt>[fmtclock [getclock] "%R"]</tt>: The tree is currently <B>"

if {$treeopen} {
    puts "<FONT SIZE=+2>OPEN</FONT></B><BR>"
} else {
    puts "<FONT SIZE=+3 COLOR=RED>CLOSED</FONT></B><BR>"
}

if {!$treeopen} {
    puts "The tree has been closed since <tt>[MyFmtClock $closetimestamp]</tt>."
}
puts "<BR>"
puts "The last known good tree had a timestamp "
puts "of <tt>[fmtclock $lastgoodtimestamp "%D %T %Z"]</tt>.<br>"

puts "<hr><pre variable>$motd</pre><hr>"


puts "<br clear=all>"

# if {[info exists FORM(whitedelta)]} {
#     set delta $FORM(whitedelta)
# } else {
#     set delta [expr 24 * 60 * 60]
# }
# 
# set fileok 0
# set filename [DataDir]/whitedelta-$delta
# if {[file exists $filename]} {
#     if {[file mtime $filename] > [file mtime [DataDir]/whiteboard]} {
#       set fileok 1
#     }
# }
# 
# if {!$fileok} {
#     set tmp [DataDir]/tmpwhite.[id process]
#     Lock
#     set date [fmtclock [expr [getclock] - $delta] "%a %b %d %H:%M:%S LT %Y"]
#     catch {exec co -q -d$date -p [DataDir]/whiteboard > $tmp 2> /dev/null}
#     catch {chmod 0666 $tmp}
#     exec ./changebar.tcl $tmp [DataDir]/whiteboard > $filename
#     unlink $tmp
#     catch {chmod 0666 $filename}
#     Unlock
# }
    

#puts "<b><a href=editwhiteboard.cgi[BatchIdPart ?]>Free-for-all whiteboard:</a></b> (Changebars indicate changes within last [PrettyDelta $delta])<pre>[html_quote [read_file $filename]]</pre><hr>"

puts "<b><a href=editwhiteboard.cgi[BatchIdPart ?]>Free-for-all whiteboard:</a></b><pre>[html_quote $whiteboard]</pre><hr>"


foreach c $checkinlist {
    upvar #0 $c info
    lappend people($info(person)) $c
    if {!$info(treeopen)} {
        lappend closedcheckin($info(person)) $c
    }
}

if {[info exists people]} {
    
    puts "The following people are on \"the hook\", since they have made"
    puts "checkins to the tree since it last opened: "
    puts "<p>"

    set peoplelist [lsort [array names people]]

    set query "(| "
    foreach p $peoplelist {
        append query "(mail=$p@netscape.com) "
        set fullname($p) ""
        set curcontact($p) ""
    }
    append query ")"

    set ldaperror 0 

    if {[catch {set fid [open "|./data/ldapsearch -b \"o=Netscape Communications Corp.,c=US\" -h $ldapserver -p $ldapport -s sub -S mail \"$query\" mail cn nscpcurcontactinfo" r]} errvar]} {
        set ldaperror 1
    } else {
        set doingcontactinfo 0
        while {[gets $fid line] >= 0} {
            if {$doingcontactinfo} {
                if {[regexp -- {^ (.*)$} $line foo n]} {
                    append curcontact($curperson) $n
                    continue
                }
                set doingcontactinfo 0
            }
            if {[regexp -- {^mail: (.*)@} $line foo n]} {
                set curperson $n
            } elseif {[regexp -- {^cn: (.*)$} $line foo n]} {
                set fullname($curperson) $n
            } elseif {[regexp -- {^nscpcurcontactinfo: (.*)$} $line foo n]} {
                set curcontact($curperson) $n
                set doingcontactinfo 1
            }
        }
        if {[catch {close $fid} errvar]} {
            set ldaperror 1
        }
    }

    if {$ldaperror} {
        puts "<font color=red>Can't contact the directory server at $ldapserver:$ldapport -- $errvar</font>"
    }

    puts "<table border cellspacing=2>"
    puts "<th colspan=2>Who</th><th>What</th><th>How to contact</th>"

    foreach p $peoplelist {
        if {[info exists closedcheckin($p)]} {
            set extra " <font color=red>([llength $closedcheckin($p)] while tree closed!)</font>"
        } else {
            set extra ""
        }

        puts "
<tr>
<td>$fullname($p)</a></td>
<td><a href=\"http://phonebook/ds/dosearch/phonebook/uid=[url_quote "$p,ou=People,o= Netscape Communications Corp.,c=US"]\">
$p</td>
<td><a href=\"showcheckins.cgi?person=$p[BatchIdPart]\">[llength $people($p)]
[Pluralize change [llength $people($p)]]</a>$extra</td>"
        puts "
<td>$curcontact($p)
</tr>"
    }

    puts "</table>"
    puts "[llength $checkinlist] checkins."

    if {[cequal $treeid default]} {
        set mailaddr "bonsai-hook"
    } else {
        set f [open data/$treeid/hooklist r]
        set fileEOF [gets $f hookentry]
        set theHookList {}
        while { $fileEOF != -1 } {
                set theHookList [concat $theHookList $hookentry]
                set fileEOF [gets $f hookentry]
        }
        set mailaddr "$theHookList"
    }
    puts "<p>"
    puts "<a href=showcheckins.cgi[BatchIdPart ?]>Show all checkins.</a><br>"
    if {[cequal $treeid default]} { 
        puts "<a href=\"mailto:[set mailaddr]@warp?subject=Build problem\">"
    } else {
        puts "<a href=\"mailto:[set mailaddr]?subject=Build problem\">"
    }
    puts "Send mail to \"the hook\".</a><br>"
} else {
    puts "Nobody seems to have made any changes since the tree opened."
}


set cvsqueryurl "cvsqueryform.cgi?cvsroot=$treeinfo($treeid,repository)&module=$treeinfo($treeid,module)"
if {[clength $treeinfo($treeid,branch)] > 0} {
    append cvsqueryurl "&branch=$treeinfo($treeid,branch)"
}

puts "
<hr>
<table>
<tr>
<th>Useful links </th><th width=10%></th><th>Help corner</th>
</tr>
<tr>
<td valign=top>
<a href=$cvsqueryurl><b>CVS Query Tool</b></a><br>
<a href=http://warp/tinderbox/showbuilds.cgi>Tinderbox continuous builds</a><br>
<a href=\"switchtree.cgi[BatchIdPart ?]\">Switch to look at a different tree or branch</a><br>
<a href=\"news:mcom.dev.client.build.busted\">Look at the Busted Tree Newsgroup</a><br>
<a href=http://phonebook/>Look up someone in the phonebook.</a><br>
<a href=viewold.cgi[BatchIdPart ?]>Time warp -- view a different day's hook.</a><br>
<a href=countcheckins.cgi[BatchIdPart ?]>See some stupid statistics about recent checkins.</a><br>
<a href=admin.cgi[BatchIdPart ?]>Administration menu.</a><br>
</td><td>
</td><td valign=top>
<a href=index.html>Introduction to Bonsai.</a><br>
<a href=http://warp/client/dogbert/tree.html>Overview of our source tree strategy.</a><br>
<a href=contacthelp.html>Changing someone else's contact info (yes you can!)</a><br>
<a href=http://warp/client/dogbert/buildlore/index.html>Dogbert build lore</a>
</td>
</tr></table>
"


exit
