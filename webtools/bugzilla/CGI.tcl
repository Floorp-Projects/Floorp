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

source "globals.tcl"

proc url_decode {buf} {
  regsub -all {\\(.)} $buf {\1} buf ; regsub -all {\\} $buf {\\\\} buf ;
  regsub -all { }  $buf {\ } buf ; regsub -all {\+} $buf {\ } buf ;
  regsub -all {\$} $buf {\$} buf ; regsub -all \n   $buf {\n} buf ;
  regsub -all {;}  $buf {\;} buf ; regsub -all {\[} $buf {\[} buf ;
  regsub -all \" $buf \\\" buf ; regsub  ^\{ $buf \\\{ buf ;
  regsub -all -nocase {%([a-fA-F0-9][a-fA-F0-9])} $buf {[format %c 0x\1]} buf
  eval return \"$buf\"
}

proc url_quote {var} {
  regsub -all { } "$var" {%20} var
  regsub -all {=} "$var" {%3d} var
  regsub -all "\n" "$var" {%0a} var
  return $var
}

proc lookup { a key } {
  global $a
  set ref [format %s(%s) $a $key]
  if { [ info exists $ref] } {
    eval return \$$ref
  } else {
    return ""
  }
}

proc ProcessFormFields {buffer} {
    global FORM MFORM
    catch {unset FORM}
    catch {unset MFORM}
    set remaining $buffer
    while {![cequal $remaining ""]} {
        if {![regexp {^([^&]*)&(.*)$} $remaining foo item remaining]} {
            set item $remaining
            set remaining ""
        }
        if {![regexp {^([^=]*)=(.*)$} $item foo name value]} {
            set name $item
            set value ""
        }
        set value [url_decode $value]
        if {![cequal $value ""]} {
            append FORM($name) $value
            lappend MFORM($name) $value
        } else {
            set isnull($name) 1
        }
    }
    if {[info exists isnull]} {
        foreach name [array names isnull] {
            if {![info exists FORM($name)]} {
                set FORM($name) ""
                set MFORM($name) ""
            }
        }
    }
}

proc FormData { field } {
  global FORM
  return $FORM($field)
}
    
if { [info exists env(REQUEST_METHOD) ] } {
  if { $env(REQUEST_METHOD) == "GET" } { 
    set buffer [lookup env QUERY_STRING]
  } else { set buffer [ read stdin $env(CONTENT_LENGTH) ] }
  ProcessFormFields $buffer
}

proc html_quote { var } {
  regsub -all {&} "$var" {\&amp;} var
  regsub -all {<} "$var" {\&lt;} var
  regsub -all {>} "$var" {\&gt;} var
  return $var
}
proc value_quote { var } {
  regsub -all {&} "$var" {\&amp;} var
  regsub -all {"} "$var" {\&quot;} var
  regsub -all {<} "$var" {\&lt;} var
  regsub -all {>} "$var" {\&gt;} var
  return $var
}

proc value_unquote { var } {
    regsub -all {&quot;} $var "\"" var
    regsub -all {&lt;} $var "<" var
    regsub -all {&gt;} $var ">" var
    regsub -all {&amp;} $var {\&} var
    return $var
}
    
foreach pair [ split [lookup env HTTP_COOKIE] ";" ] {
  set pair [string trim $pair]
  set eq [string first = $pair ]
  if {$eq == -1} {
    set COOKIE($pair) ""
  } else {
    set COOKIE([string range $pair 0 [expr $eq - 1]]) [string range $pair [expr $eq + 1] end]
  }
}

proc navigation_header {} {
  global COOKIE FORM next_bug
  set buglist [lookup COOKIE BUGLIST]
  if { $buglist != "" } {
    set bugs [split $buglist :]
    set cur [ lsearch -exact $bugs $FORM(id) ]
    puts "<B>Bug List:</B> ([expr $cur + 1] of [llength $bugs])"
    puts "<A HREF=\"show_bug.cgi?id=[lindex $bugs 0]\">First</A>"
    puts "<A HREF=\"show_bug.cgi?id=[lindex $bugs [expr [ llength $bugs ] - 1]]\">Last</A>"
    if { $cur > 0 } {
      puts "<A HREF=\"show_bug.cgi?id=[lindex $bugs [expr $cur - 1]]\">Prev</A>"
    } else {
      puts "<I><FONT COLOR=\#777777>Prev</FONT></I>"
    }
    if { $cur < [expr [ llength $bugs ] - 1] } {
      set next_bug [lindex $bugs [expr $cur + 1]]
      puts "<A HREF=\"show_bug.cgi?id=$next_bug\">Next</A>"
    } else {
      puts "<I><FONT COLOR=\#777777>Next</FONT></I>"
    }
  }
  puts "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=\"query.cgi\">Query page</A>"
}

proc make_options { src default {isregexp 0} } {
    set last "" ; set popup "" ; set found 0
    foreach item $src {
        if {$item == "-blank-" || $item != $last} {
            if { $item == "-blank-" } { set item "" }
            set last $item
            if {$isregexp ? [regexp $default $item] : [cequal $default $item]} {
                append popup "<OPTION SELECTED VALUE=\"$item\">$item"
                set found 1
            } else {
                append popup "<OPTION VALUE=\"$item\">$item"
            }
        }
    }
    if {!$found && $default != ""} {
        append popup "<OPTION SELECTED>$default"
    }
    return $popup
}



proc PasswordForLogin {login} {
    SendSQL "select password from profiles where login_name = '[SqlQuote $login]'"
    return [FetchSQLData]
}
    


proc confirm_login {{nexturl ""}} {
#    puts "Content-type: text/plain\n"
    global FORM COOKIE argv0
    ConnectToDatabase
    if { [info exists FORM(Bugzilla_login)] && 
         [info exists FORM(Bugzilla_password)] } {
        if {![regexp {^[^@, ]*@[^@, ]*\.[^@, ]*$} $FORM(Bugzilla_login)]} {
            puts "Content-type: text/html\n"
            puts "<H1>Invalid e-mail address entered.</H1>"
            puts "The e-mail address you entered"
            puts "(<b>$FORM(Bugzilla_login)</b>) didn't match our minimal"
            puts "syntax checking for a legal email address.  A legal address"
            puts "must contain exactly one '@', and at least one '.' after"
            puts "the @, and may not contain any commas or spaces."
            puts "<p>Please click <b>back</b> and try again."
            exit
        }
        set realpwd [PasswordForLogin $FORM(Bugzilla_login)]
        if {[info exists FORM(PleaseMailAPassword)]} {
            if {[cequal $realpwd ""]} {
                set realpwd [InsertNewUser $FORM(Bugzilla_login)]
            }
            set template "From: bugzilla-daemon
To: %s
Subject: Your bugzilla password.

To use the wonders of bugzilla, you can use the following:

E-mail address: %s
      Password: %s

To change your password, go to:
[Param urlbase]changepassword.cgi

(Your bugzilla and CVS password, if any, are not currently synchronized.
Top hackers are working around the clock to fix this, as you read this.)
"
            set msg [format $template $FORM(Bugzilla_login) \
                         $FORM(Bugzilla_login) $realpwd]
            
            exec /usr/lib/sendmail -t << $msg
            puts "Content-type: text/html\n"
            puts "<H1>Password has been emailed.</H1>"
            puts "The password for the e-mail address"
            puts "$FORM(Bugzilla_login) has been e-mailed to that address."
            puts "<p>When the e-mail arrives, you can click <b>Back</b>"
            puts "and enter your password in the form there."
            exit
        }
                
        if {[cequal $realpwd ""] || ![cequal $realpwd $FORM(Bugzilla_password)]} {
            puts "Content-type: text/html\n"
            puts "<H1>Login failed.</H1>"
            puts "The username or password you entered is not valid.  Please"
            puts "click <b>back</b> and try again."
            exit
        }
        set COOKIE(Bugzilla_login) $FORM(Bugzilla_login)
        set COOKIE(Bugzilla_password) $FORM(Bugzilla_password)
        puts "Set-Cookie: Bugzilla_login=$COOKIE(Bugzilla_login) ; path=/; expires=Sun, 30-Jun-2029 00:00:00 GMT"
        puts "Set-Cookie: Bugzilla_password=$COOKIE(Bugzilla_password) ; path=/; expires=Sun, 30-Jun-2029 00:00:00 GMT"
    }


    set realpwd {}

    if { [info exists COOKIE(Bugzilla_login)] && [info exists COOKIE(Bugzilla_password)] } {
        set realpwd [PasswordForLogin $COOKIE(Bugzilla_login)]
    }

    if {[cequal $realpwd ""] || ![cequal $realpwd $COOKIE(Bugzilla_password)]} {
        puts "Content-type: text/html\n"
        puts "<H1>Please log in.</H1>"
        puts "I need a legitimate e-mail address and password to continue."
        if {[cequal $nexturl ""]} {
            regexp {[^/]*$} $argv0 nexturl
        }
        set method POST
        if {[info exists env(REQUEST_METHOD)]} {
            set method $env(REQUEST_METHOD)
        }
        puts "
<FORM action=$nexturl method=$method>
<table>
<tr>
<td align=right><b>E-mail address:</b></td>
<td><input size=35 name=Bugzilla_login></td>
</tr>
<tr>
<td align=right><b>Password:</b></td>
<td><input type=password size=35 name=Bugzilla_password></td>
</tr>
</table>
"
        foreach i [array names FORM] {
            if {[regexp {^Bugzilla_} $i]} {
                continue
            }
            puts "<input type=hidden name=$i value=\"[value_quote $FORM($i)]\">"
        }
        puts "
<input type=submit value=Login name=GoAheadAndLogIn><hr>
If you don't have a password, or have forgotten it, then please fill in the
e-mail address above and click
 here:<input type=submit value=\"E-mail me a password\"
name=PleaseMailAPassword>
</form>"
        
        exit
    }
}


proc PutHeader {title h1 {h2 ""}} {
    puts "<HTML><HEAD><TITLE>$title</TITLE></HEAD>";
    puts "<BODY   BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\"";
    puts "LINK=\"#0000EE\" VLINK=\"#551A8B\" ALINK=\"#FF0000\">";

    puts [Param bannerhtml]

    puts "<TABLE BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\">";
    puts " <TR>\n";
    puts "  <TD>\n";
    puts "   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>\n";
    puts "    <TR><TD VALIGN=TOP ALIGN=CENTER NOWRAP>\n";
    puts "     <FONT SIZE=\"+3\"><B><NOBR>$h1</NOBR></B></FONT>\n";
    puts "    </TD></TR><TR><TD VALIGN=TOP ALIGN=CENTER>\n";
    puts "     <B>$h2</B>\n";
    puts "    </TD></TR>\n";
    puts "   </TABLE>\n";
    puts "  </TD>\n";
    puts "  <TD>\n";

    puts [Param blurbhtml]

    puts "</TD></TR></TABLE>\n";

}
