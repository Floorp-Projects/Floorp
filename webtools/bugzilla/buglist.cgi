#! /usr/bonsaitools/bin/mysqltcl
# -*- Mode: tcl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

puts "Content-type: multipart/x-mixed-replace;boundary=ThisRandomString"
puts ""
puts "--ThisRandomString"

proc InitMessage {str} {
    global initstr
    append initstr "$str\n"
    puts "Content-type: text/plain"
    puts ""
    puts $initstr
    puts ""
    puts "--ThisRandomString"
    flush stdout
}


# The below "if catch" stuff, if uncommented, will trap any error, and
# mail the error messages to terry.  What a hideous, horrible
# debugging hack.

# if {[catch {


source "CGI.tcl"

ConnectToDatabase

if {![info exists FORM(cmdtype)]} {
    # This can happen if there's an old bookmark to a query...
    set FORM(cmdtype) doit
}

switch $FORM(cmdtype) {
    runnamed {
        set buffer $COOKIE(QUERY_$FORM(namedcmd))
        ProcessFormFields $buffer
    }
    editnamed {
        puts "Content-type: text/html
Refresh: 0; URL=query.cgi?$COOKIE(QUERY_$FORM(namedcmd))

<TITLE>What a hack.</TITLE>
Loading your query named <B>$FORM(namedcmd)</B>..."
        exit
    }
    forgetnamed {
        puts "Set-Cookie: QUERY_$FORM(namedcmd)= ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT
Content-type: text/html

<HTML>
<TITLE>Forget what?</TITLE>
OK, the <B>$FORM(namedcmd)</B> query is gone.
<P>
<A HREF=query.cgi>Go back to the query page.</A>"
    exit
    }
    asnamed {
        if {[regexp {^[a-zA-Z0-9_ ]+$} $FORM(newqueryname)]} {
    puts "Set-Cookie: QUERY_$FORM(newqueryname)=$buffer ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT
Content-type: text/html

<HTML>
<TITLE>OK, done.</TITLE>
OK, you now have a new query named <B>$FORM(newqueryname)</B>.

<P>

<A HREF=query.cgi>Go back to the query page.</A>"
        } else {
            puts "Content-type: text/html

<HTML>
<TITLE>Picky, picky.</TITLE>
Query names can only have letters, digits, spaces, or underbars.  You entered 
\"<B>$FORM(newqueryname)</B>\", which doesn't cut it.
<P>
Click the <B>Back</B> button and type in a valid name for this query."
        }
        exit
    }
    asdefault {
        puts "Set-Cookie: DEFAULTQUERY=$buffer ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT
Content-type: text/html

<HTML>
<TITLE>OK, default is set.</TITLE>
OK, you now have a new default query.

<P>

<A HREF=query.cgi>Go back to the query page, using the new default.</A>"
        exit
    }
}

proc qadd { item } {
  global query
  append query "$item"
}


proc DefCol {name k t {s ""} {q 0}} {
    global key title sortkey needquote
    set key($name) $k
    set title($name) $t
    if {![cequal $s ""]} {
        set sortkey($name) $s
    }
    set needquote($name) $q
}

DefCol resolved_ts "bugs.resolved_ts" DateResolved bugs.resolved_ts
DefCol verified_ts "bugs.verified_ts" DateVerified bugs.verified_ts
DefCol opendate "date_format(bugs.creation_ts,'Y-m-d')" Opened bugs.creation_ts
DefCol changeddate "date_format(bugs.delta_ts,'Y-m-d')" Changed bugs.delta_ts
DefCol severity "substring(bugs.bug_severity, 1, 3)" Sev bugs.bug_severity
DefCol priority "substring(bugs.priority, 1, 3)" Pri bugs.priority
DefCol platform "substring(bugs.rep_platform, 1, 3)" Plt bugs.rep_platform
DefCol owner "assign.login_name" Owner assign.login_name
DefCol reporter "report.login_name" Reporter report.login_name
DefCol status "substring(bugs.bug_status,1,4)" State bugs.bug_status
DefCol resolution "substring(bugs.resolution,1,4)" Res bugs.resolution
DefCol summary "substring(bugs.short_desc, 1, 60)" Summary {} 1
DefCol summaryfull "bugs.short_desc" Summary {} 1
DefCol component "substring(bugs.component, 1, 8)" Comp bugs.component
DefCol product "substring(bugs.product, 1, 8)" Product bugs.product
DefCol version "substring(bugs.version, 1, 5)" Vers bugs.version
DefCol os "substring(bugs.op_sys, 1, 4)" OS bugs.op_sys
DefCol status_summary "bugs.status_summary" Status_Summary {} 1

if {[info exists COOKIE(COLUMNLIST)]} {
    set collist $COOKIE(COLUMNLIST)
} else {
    set collist $default_column_list
}

set dotweak [info exists FORM(tweak)]

if {$dotweak} {
    confirm_login
}


puts "Content-type: text/plain\n"

set query "
select
        bugs.bug_id"


foreach c $collist {
    append query ",
\t$key($c)"
}


if {$dotweak} {
    append query ",
bugs.product,
bugs.bug_status"
}

append query "
from   bugs,
       profiles assign,
       profiles report,
       versions projector
where  bugs.assigned_to = assign.userid 
and    bugs.reporter = report.userid
and    bugs.product = projector.program
and    bugs.version = projector.value
"

if {[info exists FORM(sql)]} {
  append query "and (\n[join [url_decode $FORM(sql)] { }]\n)"
} else {


  set legal_fields { bug_id product version rep_platform op_sys bug_status
                     resolution priority bug_severity assigned_to reporter
                     bug_file_loc short_desc component
                     status_summary resolved_ts verified_ts}

  foreach field [array names FORM] {
    if { [ lsearch $legal_fields $field ] != -1 && ![cequal $FORM($field) ""]} {
      qadd "\tand (\n"
      set or ""
      if { $field == "assigned_to" || $field == "reporter" || $field == "qa_assigned_to"} {
        foreach p [split $FORM($field) ","] {
          qadd "\t\t${or}bugs.$field = [DBname_to_id $p]\n"
          set or "or "
        }
      } elseif { $field == "resolved_ts"} {
                        if {! [cequal $FORM(resolved_ts_2) ""]} { 
                                qadd "\t\tbugs.resolved_ts between \n\t\t\tTO_DATE('$FORM($field)','DD-MON-YY') and\n \t\t\tTO_DATE('$FORM(resolved_ts_2)', 'DD-MON-YY')\n"
                        } else {
                                qadd "\t\tTO_CHAR (bugs.resolved_ts,'DD-MON-YY') = '[string toupper $FORM($field)]'\n"
                                }
      } elseif { $field == "verified_ts"} {
                        if {! [cequal $FORM(verified_ts_2) ""]} {
                                qadd "\t\tbugs.verified_ts between \n\t\t\tTO_DATE('$FORM($field)','DD-MON-YY') and\n \t\t\tTO_DATE('$FORM(verified_ts_2)', 'DD-MON-YY')\n"
                        } else {
                                qadd "\t\tTO_CHAR (bugs.verified_ts,'DD-MON-YY') = '[string toupper $FORM($field)]'\n"
                                }
      } else {
        foreach v $MFORM($field) {
          if {[cequal $v "(empty)"]} {
              qadd "\t\t${or}bugs.$field is null\n"
          } else {
              qadd "\t\t${or}bugs.$field = '$v'\n"
          }
          set or "or "
        }
      }
      qadd "\t)\n"
    }
  }

  if {[lookup FORM changedin] != ""} {
      set c [string trim $FORM(changedin)]
      if {$c != ""} {
          if {![regexp {^[0-9]*$} $c]} {
              puts "
The 'changed in last ___ days' field must be a simple number.  You entered 
\"$c\", which doesn't cut it.

Click the Back button and try again."
              exit
          }
          qadd "and to_days(now()) - to_days(bugs.delta_ts) <= $FORM(changedin) "
      }
  }
}

if {[info exists FORM(order)]} {
    qadd "order by "
    switch -glob $FORM(order) {
        *.* {}
        *Number* {
            set FORM(order) bugs.bug_id
        }
        *Import* {
            set FORM(order) bugs.priority
        }
        *Assign* {
            set FORM(order) "assign.login_name, bugs.bug_status, priorities.rank, bugs.bug_id"
        }
        default {
            set FORM(order) "bugs.bug_status, priorities.rank, assign.login_name, bugs.bug_id"
        }
    }
    if {[cequal [cindex $FORM(order) 0] "\{"]} {
        # I don't know why this happens, but...
        set FORM(order) [lindex $FORM(order) 0]
    }
    qadd $FORM(order)
}

puts "Please stand by ..."
if {[info exists FORM(debug)]} {
    puts $query
}
flush stdout
set child 0
if {[info exists FORM(keepalive)]} {
  set child [fork]
  if {$child == 0} {
    while 1 {
      puts "Still waiting ..."
      flush stdout
      sleep 10
    }
    puts "Child process died, what's up?"
    flush stdout
    exit 0
  }
}
SendSQL $query

set count 0
set bugl ""
proc pnl { str } {
  global bugl
  append bugl "$str"
}

regsub -all {[&?]order=[^&]*} $buffer {} fields
regsub -all {[&?]cmdtype=[^&]*} $fields {} fields


if {[info exists FORM(order)]} {
    regsub -all { } ", $FORM(order)" "%20" oldorder
} else {
    set oldorder ""
}

if {$dotweak} {
    pnl "<FORM NAME=changeform METHOD=POST ACTION=\"process_bug.cgi\">"
}

set tablestart "<TABLE CELLSPACING=0 CELLPADDING=2>
<TR ALIGN=LEFT><TH>
<A HREF=\"buglist.cgi?[set fields]&order=bugs.bug_id\">ID</A>"


foreach c $collist {
    if {$needquote($c)} {
        append tablestart "<TH WIDTH=100% valigh=left>"
    } else {
        append tablestart "<TH valign=left>"
    }
    if {[info exists sortkey($c)]} {
        append tablestart "<A HREF=\"buglist.cgi?[set fields]&order=$sortkey($c)$oldorder\">$title($c)</A>"
    } else {
        append tablestart $title($c)
    }
}

append tablestart "\n"

set dotweak [info exists FORM(tweak)]

set p_true 1

while { $p_true } {
    set result [FetchSQLData]
    set p_true [MoreSQLData]
    if { $result != "" } { 
        set bug_id [lvarpop result]
        if {![info exists seen($bug_id)]} {
            set seen($bug_id) 1
            incr count
            if {($count % 200) == 0} {
                # Too big tables take too much browser memory...
                pnl "</TABLE>$tablestart"
            }
            if {[info exists buglist]} {
                append buglist ":$bug_id"
            } else {
                set buglist $bug_id
            }
            pnl "<TR VALIGN=TOP ALIGN=LEFT><TD>"
            if {$dotweak} {
                pnl "<input type=checkbox name=id_$bug_id>"
            }
            pnl "<A HREF=\"show_bug.cgi?id=$bug_id\">"
            pnl "$bug_id</A> "
            foreach c $collist {
                set value [lvarpop result]
                set nowrap {} 

                #-- This cursor is used to pick the login_name to be
                #   displayed on the query list as the field value may or
                #   maynot have vales associated to it

                if { $c == "qa_assigned_to"} {
                     set dml_cur [ oraopen $lhandle ]

                     orasql $dml_cur "select login_name 
                                      from   profiles 
                                      where  userid = $value"

                     set cur_resultset [orafetch $dml_cur]

                     if {$cur_resultset != ""} { 
                         set  value $cur_resultset 
                         set nowrap {nowrap}
                     } else {
                         set  value ""
                     }

                     oraclose $dml_cur

                } 

                if {$needquote($c)} {
                    set value [html_quote $value]
                } else {
                    set value "<nobr>$value</nobr>"
                }
                pnl "<td $nowrap>$value"
            }
            if {$dotweak} {
                set value [lvarpop result]
                set prodarray($value) 1
                set value [lvarpop result]
                set statusarray($value) 1
            }
            pnl "\n"
        }
    }
}
if {$child != 0} {
  kill $child
}
puts ""
puts "--ThisRandomString"

set toolong 0
puts "Content-type: text/html"
if { [info exists buglist] } {
    if {[clength $buglist] < 4000} {
        puts "Set-Cookie: BUGLIST=$buglist\n"
    } else {
        puts "Set-Cookie: BUGLIST=\n"
        set toolong 1
    }
} else {
  puts ""
}
set env(TZ) PST8PDT

PutHeader "Bug List" "Bug List"

puts -nonewline "
<CENTER><H1>M<font -= 2>OZILLA</font> B<font -= 2>UGS</font></H1>
<B>[fmtclock [getclock ]]</B>"
if {[info exists FORM(debug)]} { puts "<PRE>$query</PRE>" }

if {$toolong} {
    puts "<h2>This list is too long for bugzilla's little mind; the"
    puts "Next/Prev/First/Last buttons won't appear.</h2>"
}

set cdata [ split [read_file -nonewline "comments"] "\n" ]
random seed
puts {<HR><I><A HREF="newquip.html">}
puts [lindex $cdata [random [llength $cdata]]]</I></A></CENTER>
puts "<HR SIZE=10>$tablestart"
puts $bugl
puts "</TABLE>"

switch $count {
    0 {
        puts "Zarro Boogs found."
    }
    1 {
        puts "One bug found."
    }
    default {
        puts "$count bugs found."
    }
}

if {$dotweak} {
    GetVersionTable
    puts "
<SCRIPT>
numelements = document.changeform.elements.length;
function SetCheckboxes(value) {
    for (var i=0 ; i<numelements ; i++) {
        item = document.changeform.elements\[i\];
        item.checked = value;
    }
}
document.write(\" <input type=button value=\\\"Uncheck All\\\" onclick=\\\"SetCheckboxes(false);\\\"> <input type=button value=\\\"Check All\\\" onclick=\\\"SetCheckboxes(true);\\\">\");
</SCRIPT>"
    set resolution_popup [make_options $legal_resolution_no_dup FIXED]
    GetVersionTable
    set prod_list [array names prodarray]
    set list $prod_list
    set legal_target_versions $versions([lvarpop list])
    foreach p $list {
        set legal_target_versions [intersect $legal_target_versions \
                                       $versions($p)]
    }
    set version_popup [make_options \
                              [concat "-blank-" $legal_target_versions] \
                              $dontchange]
    set platform_popup [make_options $legal_platform $dontchange]
    set priority_popup [make_options $legal_priority $dontchange]
    set sev_popup [make_options $legal_severity $dontchange]
    if {[llength $prod_list] == 1} {
        set prod_list [lindex $prod_list 0 ]
        set legal_component [linsert $components($prod_list) 0 { }]
    } else {
        set legal_component { }
    }

    set component_popup [make_options $legal_component $dontchange]

    set product_popup [make_options $legal_product $dontchange]


    puts "
<hr>
<TABLE>
<TR>
    <TD ALIGN=RIGHT><B>Product:</B></TD>
    <TD><SELECT NAME=product>$product_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Version:</B></TD>
    <TD><SELECT NAME=version>$version_popup</SELECT></TD>
<TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></TD>
    <TD><SELECT NAME=rep_platform>$platform_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority:</A></B></TD>
    <TD><SELECT NAME=priority>$priority_popup</SELECT></TD>
</TR>
<TR>
    <TD ALIGN=RIGHT><B>Component:</B></TD>
    <TD><SELECT NAME=component>$component_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity:</A></B></TD>
    <TD><SELECT NAME=bug_severity>$sev_popup</SELECT></TD>
</TR>
</TABLE>

<INPUT NAME=multiupdate value=Y TYPE=hidden>

<B>Additional Comments:</B>
<BR>
<TEXTAREA WRAP=HARD NAME=comment ROWS=5 COLS=80></TEXTAREA><BR>"

    # knum is which knob number we're generating, in javascript terms.

    set knum 0
    puts "
<INPUT TYPE=radio NAME=knob VALUE=none CHECKED>
        Do nothing else<br>"
    incr knum
    puts "
<INPUT TYPE=radio NAME=knob VALUE=accept>
        Accept bugs (change status to <b>ASSIGNED</b>)<br>"
    incr knum
    if {![info exists statusarray(CLOSED)] && \
            ![info exists statusarray(VERIFIED)] && \
            ![info exists statusarray(RESOLVED)]} {
        puts "
<INPUT TYPE=radio NAME=knob VALUE=clearresolution>
        Clear the resolution<br>"
        incr knum
        puts "
<INPUT TYPE=radio NAME=knob VALUE=resolve>
        Resolve bugs, changing <A HREF=\"bug_status.html\">resolution</A> to
        <SELECT NAME=resolution
          ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\">
          $resolution_popup</SELECT><br>"
        incr knum
    }
    if {![info exists statusarray(NEW)] && \
            ![info exists statusarray(ASSIGNED)] && \
            ![info exists statusarray(REOPENED)]} {
        puts "
<INPUT TYPE=radio NAME=knob VALUE=reopen> Reopen bugs<br>"
        incr knum
    }
    if {[llength [array names statusarray]] == 1} {
        if {[info exists statusarray(RESOLVED)]} {
            puts "
<INPUT TYPE=radio NAME=knob VALUE=verify>
        Mark bugs as <b>VERIFIED</b><br>"
            incr knum
        }
        if {[info exists statusarray(VERIFIED)]} {
            puts "
<INPUT TYPE=radio NAME=knob VALUE=close>
        Mark bugs as <b>CLOSED</b><br>"
            incr knum
        }
    }
    puts "
<INPUT TYPE=radio NAME=knob VALUE=reassign> 
        <A HREF=\"bug_status.html#assigned_to\">Reassign</A> bugs to
        <INPUT NAME=assigned_to SIZE=32
          ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\"
          VALUE=\"$COOKIE(Bugzilla_login)\"><br>"
    incr knum
    puts "<INPUT TYPE=radio NAME=knob VALUE=reassignbycomponent>
          Reassign bugs to owner of selected component<br>"
    incr knum

    puts "
<p>
<font size=-1>
To make changes to a bunch of bugs at once:
<ol>
<li> Put check boxes next to the bugs you want to change.
<li> Adjust above form elements.  (It's <b>always</b> a good idea to add some
     comment explaining what you're doing.)
<li> Click the below \"Commit\" button.
</ol></font>
<INPUT TYPE=SUBMIT VALUE=Commit>
</FORM><hr>"
}


if {$count > 0} {
  puts "<FORM METHOD=POST ACTION=\"long_list.cgi\">
<INPUT TYPE=HIDDEN NAME=buglist VALUE=$buglist>
<INPUT TYPE=SUBMIT VALUE=\"Long Format\">
<A HREF=\"query.cgi\">Query Page</A>
<A HREF=\"colchange.cgi?$buffer\">Change columns</A>
</FORM>"
    if {!$dotweak && $count > 1} {
        puts "<A HREF=\"buglist.cgi?[set fields]&tweak=1\">Make changes to several of these bugs at once.</A>"
    }
}
puts "--ThisRandomString--"
flush stdout

#
# Below is second part of hideous "if catch" stuff from above. 
#
#
# 
# }]} {
#     exec /usr/lib/sendmail -t << "To: terry
# 
# 
# $query
# 
# $errorInfo
# "
# }
