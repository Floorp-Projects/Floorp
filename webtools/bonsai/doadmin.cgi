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
source adminfuncs.tcl

puts "Content-type: text/html

"

CheckPassword $FORM(password)

Lock
LoadCheckins



switch -exact -- $FORM(command) {
    close {
        AdminCloseTree [ParseTimeAndCheck [FormData closetimestamp]]

        puts "
<TITLE>Clang!</TITLE>
<H1>The tree is now closed.</H1>
Mail has been sent notifying \"the hook\" and anyone subscribed to
bonsai-treeinterest.
<P>
<a href=\"mailto:clienteng?subject=The tree is now closed.\">Click here</a>
to send e-mail about it to clienteng.
"

    }
    open {
        AdminOpenTree [ParseTimeAndCheck [FormData lastgood]] \
            [info exists FORM(doclear)]
        puts "
<TITLE>The floodgates are open.</TITLE>
<H1>The tree is now open.</H1>
Mail has been sent notifying \"the hook\" and anyone subscribed to
bonsai-treeinterest.
<a href=\"mailto:clienteng?subject=The tree is now opened.\">Click here</a>
to send e-mail about it to clienteng.
"
    }
    tweaktimes {
        set lastgoodtimestamp [ParseTimeAndCheck [FormData lastgood]]
        set closetimestamp [ParseTimeAndCheck [FormData lastclose]]
        puts "
<TITLE>Let's do the time warp again...</TITLE>
<H1>Times have been tweaked.</H1>
"
        Log "Times tweaked: lastgood is [MyFmtClock $lastgoodtimestamp], closetime is [MyFmtClock $closetimestamp]"
    }
    editmotd {
        LoadMOTD
        if {![cequal [FormData origmotd] $motd]} {
            puts "
<TITLE>Oops!</TITLE>
<H1>Someone else has been here!</H1>

It looks like somebody else has changed the message-of-the-day.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the top of Bonsai,
look at the current message-of-the-day, and decide if you still
want to make your edits."
            PutsTrailer
            exit
        }

        MailDiffs "message-of-the-day" $motd [FormData motd]
        set motd [FormData motd]
        puts "
<TITLE>New MOTD</TITLE>
<H1>The message-of-the-day has been changed.</H1>
"
        WriteMOTD
        Log "New motd: $motd"
    }
    changepassword {
        if {![cequal $FORM(newpassword) $FORM(newpassword2)]} {
            puts "
<TITLE>Oops!</TITLE>
<H1>Mismatch!</H1>
The two passwords you typed didn't match.  Click <b>Back</b> and try again."
            PutsTrailer
            exit
        }
        if {$FORM(doglobal)} {
            CheckGlobalPassword $FORM(password)
            set outfile data/passwd
        } else {
            set outfile "[DataDir]/treepasswd"
        }
        set encoded [string trim [exec ./data/trapdoor $FORM(newpassword)]]
        set fid [open $outfile "w"]
        puts $fid $encoded
        close $fid
        catch {chmod 0777 $outfile}
        puts "
<TITLE>Locksmithing complete.</TITLE>
<H1>Password changed.</H1>
The new password is now in effect."
        PutsTrailer
        exit
    }
}


PutsTrailer

WriteCheckins
Unlock

exit
