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
LoadTreeConfig
Unlock


# Stupid hack to make an empty array:
set peoplearray(zzz) 1
unset peoplearray(zzz)
set versioninfo ""

proc BreakBig {str} {
    set result {}
    while {[clength $str] > 20} {
        set head [crange $str 0 19]
        set w [string last "/" $head]
        if {$w < 0} {
            set w 19
        }
        append result "[crange $str 0 $w]<br>"
        incr w
        set str [crange $str $w end]
    }
    append result $str
}
        
        

set tweak [info exists FORM(tweak)]

set delta_size 1 ;#[info exists FORM(delta_size)]

puts "Content-type: text/html"

if {[info exists FORM(sort)]} {
    puts "Set-Cookie: SORT=$FORM(sort)"
} elseif {[info exists COOKIE(SORT)]} {
    set FORM(sort) $COOKIE(SORT)
} else {
    set FORM(sort) date
}

puts "
<HTML>"

if {[info exists FORM(person)]} {
    puts "<TITLE>Checkins for $FORM(person)</TITLE>"
    puts "<H1>Checkins for $FORM(person)</H1>"
    set list {}
    foreach i $checkinlist {
        upvar #0 $i info
        if {[cequal $info(person) $FORM(person)]} {
            lappend list $i
        }
    }
} elseif {[info exists FORM(mindate)] || [info exists FORM(maxdate)]} {
    set str "Checkins"
    set min 0
    set max [expr 1<<30]
    if {[info exists FORM(mindate)]} {
        set min $FORM(mindate)
        append str " since [fmtclock $min "%m/%d %H:%M"]"
        if {[info exists FORM(maxdate)]} {
            append str " and"
        }
    }
    if {[info exists FORM(maxdate)]} {
        set max $FORM(maxdate)
        append str " before [fmtclock $max "%m/%d %H:%M"]"
    }
    puts "<TITLE>$str</TITLE>"
    puts "<H1>$str</H1>"
    set list {}
    foreach i $checkinlist {
        upvar #0 $i info
        if {$info(date) >= $min && $info(date) <= $max} {
            lappend list $i
        }
    }
} else {
    puts "<TITLE>All checkins</TITLE>"
    puts "<H1>All Checkins</H1>"
    set list $checkinlist
}

if {$readonly} {
    puts "<h2><font color=red>Be aware that you are looking at an old hook!</font></h2>"
}


puts "(Current sort is by <tt>$FORM(sort)</tt>; click on a column header
to sort by that column.)"

# Oh, boy, is this ever gross.  Dynamically write some code to be the sort
# comparison routine, so that we know that the sort code will run fast.

set fields [split $FORM(sort) ","]
set w [lsearch $fields "date"]
if {$w >= 0} {
    set fields [lrange $fields 0 [expr $w - 1]]
}

set body {
    upvar #0 $n1 a $n2 b
}
foreach i $fields {
    append body "set delta \[string compare \$a($i) \$b($i)\]"
    append body "\n"
    append body {if {$delta != 0} {return $delta}}
    append body "\n"
}
append body {return [expr $b(date) - $a(date)]}

eval [list proc Compare {n1 n2} $body]

set total_added 0
set total_removed 0

#
# Calculate delta information
#
if {$delta_size} {
    foreach i $list {
        upvar #0 $i info
        set info(added) 0
        set info(removed) 0

        #
        # Loop through the checkins, grab the filename and stickyflags
        #
        if {[info exists info(fullinfo)]} {
            foreach fu $info(fullinfo) {
                set fn [lindex $fu 0]
                set sticky [lindex $fu 4]

                #
                # if the file is binary, don't show the delta information
                #
                if {    ![string match {*.gif} $fn]
                             && ![string match {*.bmp} $fn]
                             && ![string match {-kb} $sticky]} {
                    scan [lindex $fu 2] {%d} file_added
                    scan [lindex $fu 3] {%d} file_removed
                    if {[info exists file_added] && [info exists file_removed]} {
                        incr info(added) $file_added
                        incr info(removed) $file_removed
                    }
                }
            }
        }

        set info(lines_changed) [format "%7d" [expr 1000000 - ($info(added) - $info(removed))]]
        incr total_added $info(added)
        incr total_removed $info(removed)
    }
}

set list [lsort -command Compare $list]

regsub -all {[&?]sort=[^&]*} $buffer {} otherparams

proc NewSort {key} {
    global otherparams FORM
    set list [split $FORM(sort) ","]
    set w [lsearch $list $key]
    if {$w >= 0} {
        set list [lreplace $list $w $w]
    }
    set list [linsert $list 0 $key]
    return "[set otherparams]&sort=[join $list ,]"
}


if {$tweak} {
    puts "<FORM method=get action=\"dotweak.cgi\">"
}


puts "
<TABLE border cellspacing=2>
<TR ALIGN=LEFT>
"

if {$tweak} {
    puts "<TH></TH>"
}

puts "
<TH><A HREF=\"showcheckins.cgi?[set otherparams]&sort=date\">When</A>
<TH><A HREF=\"showcheckins.cgi?[NewSort treeopen]\">Tree state</A>
<TH><A HREF=\"showcheckins.cgi?[NewSort person]\">Who</A>
<TH><A HREF=\"showcheckins.cgi?[NewSort dir]\">Directory</A>
<TH><A HREF=\"showcheckins.cgi?[NewSort files]\">Files</A>"

if {$delta_size} {
    puts "<TH><A HREF=\"showcheckins.cgi?[NewSort lines_changed]\"><tt>+/-</tt></A>"
}

puts "
<TH WIDTH=100%>Description
</TR>"

set count 0
set maxcount 100

set branchpart {}

if {![cequal $treeinfo($treeid,branch) {}]} {
    set branchpart "&branch=$treeinfo($treeid,branch)"
}

foreach i $list {
    upvar #0 $i info
    incr count
    if {$count >= $maxcount} {
        set count 0
        # Don't make tables too big, or toy computers will break.
        puts "</TABLE><TABLE border cellspacing=2>"
    }
    puts "<TR>"
    if {$tweak} {
        puts "<TD><INPUT TYPE=CHECKBOX NAME=$i></TD>"
    }
    puts "<TD><a href=editcheckin.cgi?id=$i[BatchIdPart]>"
    puts "[fmtclock $info(date) "<font size=-2>%m/%d %H:%M</font>"]</a></TD>"
    puts "<TD>"
    if {$info(treeopen)} {
        puts "open"
    } else {
        puts "CLOSED"
    }
    if {[info exists info(notes)]} {
        if {![cequal $info(notes) ""]} {
            puts "<br>$info(notes)"
        }
    }
    puts "</TD>"
    set peoplearray($info(person)) 1
    puts "<TD><a href=\"http://phonebook/ds/dosearch/phonebook/uid=[url_quote "$info(person),ou=People,o= Netscape Communications Corp.,c=US"]\">$info(person)</a></TD>"
    puts "<TD><a href=\"cvsview2.cgi?root=$treeinfo($treeid,repository)&subdir=$info(dir)&files=[join $info(files) +]&command=DIRECTORY$branchpart\">[BreakBig $info(dir)]</a></TD>"
    puts "<TD>"
    foreach f $info(files) {
        puts "<a href=\"cvsview2.cgi?root=$treeinfo($treeid,repository)&subdir=$info(dir)&files=$f&command=DIRECTORY$branchpart\">$f</a>"
    }

    puts "</TD>"
    if {$delta_size} {
        puts "<TD>"
        if {$info(removed) < 0} {
            set str_removed $info(removed)
        } else {
            set str_removed "-0"
        }
        puts "<tt>+$info(added)<br>$str_removed"
        puts "</TD>"
    }
    if {[info exists info(fullinfo)]} {
        foreach f $info(fullinfo) {
            lassign $f file version
            append versioninfo "$info(person)|$info(dir)|$file|$version,"
        }
    }
    puts "<TD WIDTH=100%>$info(log)</TD>"
    puts "</TR>"
}
puts "</TABLE>"


if {$delta_size} {
     set deltastr " &nbsp;&nbsp;&nbsp; Lines changed <tt>($total_added/$total_removed)</tt>."
} else {
     set deltastr ""
}


puts "[llength $list] checkins listed. $deltastr"


if {$tweak} {
    puts "
<hr>
Check the checkins you wish to affect.  Then select one of the below options.
And type the magic word.  Then click on submit.
<P>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<INPUT TYPE=radio NAME=command VALUE=nuke>Delete these checkins.<BR>
<INPUT TYPE=radio NAME=command VALUE=setopen>Set the tree state on these checkins to be <B>Open</B>.<BR>
<INPUT TYPE=radio NAME=command VALUE=setclose>Set the tree state on these checkins to be <B>Closed</B>.<BR>
<INPUT TYPE=radio NAME=command VALUE=movetree>Move these checkins over to this tree:
<SELECT NAME=desttree SIZE=1>"

    proc IsSelected {value} {
        global treeid
        if {[cequal $value $treeid]} {
            return "SELECTED"
        } else {
            return ""
        }
    }
        

    foreach i $treelist {
        if {![info exists treeinfo($i,nobonsai)]} {
            puts "<OPTION [IsSelected $i] VALUE=$i>$treeinfo($i,description)"
        }
    }
        
    puts "</SELECT><P>
<B>Password:</B><INPUT NAME=password TYPE=password></td>
<BR>
<INPUT TYPE=SUBMIT VALUE=Submit>
</FORM>"
} else {
    puts "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
    puts "<a href=showcheckins.cgi?$buffer&tweak=1>Tweak some of these checkins.</a>"
    puts "<br><br>"
    puts "<FORM action='multidiff.cgi' method=post>"
    puts "<INPUT TYPE='HIDDEN' name='allchanges' value = '$versioninfo'>"
    puts "<INPUT TYPE=SUBMIT VALUE='Show me ALL the Diffs'>"
    puts "</FORM>"
}

if {[info exists FORM(ltabbhack)]} {
    puts "<!-- StupidLloydHack [join [lsort [array names peoplearray]] {,}] -->"
    puts "<!-- LloydHack2 $versioninfo -->"
}


PutsTrailer
