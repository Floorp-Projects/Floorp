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

# What some of the global variables mean:
# lastgoodtimestamp -- the timestamp when we last knew we had a good tree.
# closetimestamp -- when the tree was closed.  When we open, it probably
#     becomes the lastgoodtimestamp

set cvscommand /tools/ns/bin/cvs
set rlogcommand /tools/ns/bin/rlog
set rcsdiffcommand /tools/ns/bin/rcsdiff
set cocommand /tools/ns/bin/co
set lxr_base http://cvs-mirror.mozilla.org/webtools/lxr/source
set mozilla_lxr_kludge TRUE

set ldapserver nsdirectory.mcom.com
# set ldapserver hoth.mcom.com
set ldapport 389


if {![info exists env(TZ)] || [cequal $env(TZ) ""]} {
    # Shouldn't have to do this!  Something busted on warp, I think.
    set env(TZ) PST8PDT
}


# BUGSYSTEMEXPR is something that may be redefined in data/configdata, which
# will define what to use in the replacement part of a regsub to quote bug
# numbers that appear in the system.
set BUGSYSTEMEXPR {<A HREF="http://scopus/bugsplat/show_bug.cgi?id=&">&</A>}
set treeid {default}


if {[info exists tcl_version] && $tcl_version >= 8.0} {
    # The below tclX functions moved into the main TCL codebase in version 8,
    # but their names changed.  So, we cope.  Note that these conversion
    # routines only cover Bonsai's current use of these routines, not all
    # possible uses.
    proc getclock {} {
        return [clock seconds]
    }
    proc convertclock {str args} {
        if {[lindex $args 0] == "GMT"} {
            return [clock scan $str -gmt 1]
        }
        return [clock scan $str]
    }
    proc fmtclock {date args} {
    }
}
    


proc NOTDEF {foo} {
}

proc ConnectToDatabase {} {
    global mysqlhandle
    if {![info exists mysqlhandle]} {
        set mysqlhandle [mysqlconnect]
        mysqluse $mysqlhandle "bonsai"
    }
}

proc SendSQL { str } {
# puts $str
    global mysqlhandle errorInfo
    if {[catch {mysqlsel $mysqlhandle $str} errmsg]} {
        puts $str
        error "$errmsg - $str" $errorInfo
    }
    return 0
}

proc MoreSQLData {} {
    global mysqlhandle
    set result [mysqlresult $mysqlhandle "rows?"]
    return [expr ![cequal $result ""] && $result > 0]
}

proc FetchSQLData {} {
    global mysqlhandle
    return [mysqlnext $mysqlhandle]
}

proc SqlQuote {str} {
    regsub -all "'" $str "''" str
    # 
    # This next line is quoting hell.  One level of quoting comes from
    # the TCL interpreter, and another level comes from TCL's regular
    # expression parser.  It really works out to "change every
    # backslash to two backslashes".
    regsub -all "\\\\" $str "\\\\\\\\" str

    return $str
}


# proc GetId {table field value} {
#     global lastidcache
#     if {[info exists lastidcache($table)]} {
#         lassign lastidcache($table) cval id
#         if {[cequal $value $cval]} {
#             return $id
#         }
#     }
#     set qvalue [SqlQuote $value]
#     SendSQL "select id from $table where $field = '$qvalue'"
#     set result [lindex [FetchSQLData] 0]
#     if {[cequal $result ""]} {
#         SendSQL "insert into $table ($field) values ('$qvalue')"
#         SendSQL "select LAST_INSERT_ID()"
#         set result [lindex [FetchSQLData] 0]
#     }
#     set lastidcache($table) [list $value $result]
#     return $result
# }

proc GetId {table field value} {
    global lastidcache
    if {[info exists lastidcache($table,$field,$value)]} {
        set id $lastidcache($table,$field,$value)
        return $id
    }
    set qvalue [SqlQuote $value]
    SendSQL "select id from $table where $field = '$qvalue'"
    set result [lindex [FetchSQLData] 0]
    if {[cequal $result ""]} {
        SendSQL "insert into $table ($field) values ('$qvalue')"
        SendSQL "select LAST_INSERT_ID()"
        set result [lindex [FetchSQLData] 0]
    }
    set lastidcache($table,$field,$value) $result
    return $result
}


set lastdescription { }
set lastdescriptionid 0
proc AddToDatabase {lines desc} {
    global lastdescription lastdescriptionid
    if {[clength $desc] > 60000} {
        set desc [crange $desc 0 60000]
    }
    set desc [string trimright $desc]
    
    if {[cequal $desc $lastdescription]} {
        set descid $lastdescriptionid
    } else {
        set descid {}
    }

    set basequery "replace into checkins(type,when,whoid,repositoryid,dirid,"
    append basequery "fileid,revision,stickytag,branchid,addedlines,"
    append basequery "removedlines,descid) values ("
    foreach line [split $lines "\n"] {
        if {[cequal $line ""]} {
            continue
        }
        lassign [split $line "|"] chtype date name repository dir \
            file version sticky branch addlines removelines

        regsub {^T} $branch {} branch
        regsub {/$} $dir {} dir
        if {[cequal $addlines ""]} {
            set addlines 0
        }
        if {[cequal $removelines ""]} {
            set removelines 0
        }
        if {[catch {set removelines [expr abs($removelines)]}]} {
            continue
        }
        if {[catch {set date [fmtclock $date {%Y-%m-%d %H:%M}]}]} {
            continue
        }


        if {[cequal $descid ""]} {
            set quoted [SqlQuote $desc]
            SendSQL "select distinct descid from checkins,descs where when = '$date' and descs.id = descid and descs.description = '$quoted'"
            if {![MoreSQLData]} {
                SendSQL "insert into descs (description) values ('$quoted')"
                SendSQL "select LAST_INSERT_ID()"
            }
            set descid [lindex [FetchSQLData] 0]
            set lastdescriptionid $descid
            set lastdescription $desc
        }

        set query $basequery
        switch $chtype {
            "C" {
                append query "'Change'"
            }
            "A" {
                append query "'Append'"
            }
            "R" {
                append query "'Remove'"
            }
            default {
                append query "NULL"
            }
        }
        append query ",'$date'"
        append query ",[GetId people who $name]"
        append query ",[GetId repositories repository $repository]"
        append query ",[GetId dirs dir $dir]"
        append query ",[GetId files file $file]"
        append query ",'[SqlQuote $version]'"
        append query ",'[SqlQuote $sticky]'"
        append query ",[GetId branches branch $branch]"
        append query ",$addlines"
        append query ",$removelines"
        append query ",$descid)"
        SendSQL $query
    }
}
        
        
            
        


    


proc assert {arg} {
    set result [uplevel 1 expr $arg]
    if {!$result} {
        error "Bad assertion $arg"
    }
}
set lockcount 0


proc html_quote {var} {
    regsub -all {&} "$var" {\&amp;} var
    regsub -all {<} "$var" {\&lt;} var
    regsub -all {>} "$var" {\&gt;} var
    return $var
}

proc url_quote_char {c} {
    scan $c "%c" value
    if {$value <= 32 || [regexp {[ %=&?]} $c]} {
        return [format "%%%02x" $value]
    }
    return $c
}

proc url_quote {var} {
    set result ""
    foreach c [split $var ""] {
        append result [url_quote_char $c]
    }
    return $result
}

proc DataDir {} {
    global treeid
    if {[cequal $treeid "default"]} {
        return data
    } else {
        return data/$treeid
    }
}


proc Lock {} {
    global lockcount lockfid
    if {$lockcount <= 0} {
        set lockcount 0
        if {[catch {set lockfid [open "data/lockfile" "a"]}]} {
            catch {mkdir data}
            catch {chmod 0777 data}
            set lockfid [open "data/lockfile" "a"]
        }
        flock -write $lockfid
        catch {chmod 0666 data/lockfile}
    }
    incr lockcount
}


proc Unlock {} {
    global lockcount lockfid
    incr lockcount -1
    if {$lockcount <= 0} {
        funlock $lockfid
        close $lockfid
    }
}


proc LoadDirList {} {
    global legaldirs treeid treeinfo
    set legaldirs {}

    if {![info exists treeinfo($treeid,repository)]} {
        LoadTreeConfig
    }
    
    set modules $treeinfo($treeid,repository)/CVSROOT/modules
    set dirsfile [DataDir]/legaldirs

    if {[file exists $modules]} {
        if {![file exists $dirsfile] ||
            [file mtime $dirsfile] < [file mtime $modules]} {
            catch {exec ./createlegaldirs.tcl $treeid}
        }
    }
            
    Lock
    for_file line $dirsfile {
        lappend legaldirs $line
    }
    Unlock
}


proc PickNewBatchID {} {
    global batchid
    incr batchid
    Lock
    set fid [open [DataDir]/batchid "w"]
    puts $fid "set batchid $batchid"
    close $fid
    Unlock
}



set readonly 0

proc LoadCheckins {} {
    global batchid treeopen checkinlist
    global lastgoodtimestamp closetimestamp
    assert {![info exists checkinlist]}
    Lock
    if {![info exists batchid]} {
        set filename "[DataDir]/batchid"
        if {![file exists $filename]} {
            set fid [open $filename "w"]
            chmod 0666 $filename
            puts $fid "set batchid 1"
            close $fid
        }
        uplevel #0 source $filename
    }
    set filename [DataDir]/batch-$batchid
    if {[file exists $filename]} {
        uplevel #0 source $filename
    }
    Unlock

    if {![info exists checkinlist]} {
        set checkinlist {}
    }
    if {![info exists treeopen]} {
        set treeopen 1
    }
    foreach t {lastgoodtimestamp closetimestamp} {
        if {![info exists $t]} {
            set $t [convertclock "1/1/70"]
        }
    }
}


proc WriteCheckins {} {
    global batchid checkinlist treeopen readonly
    global lastgoodtimestamp closetimestamp 
    if {$readonly} {
        puts "<P><B><font color=red>Can't write checkins file; not viewing"
        puts "current info.</font></b>"
        return
    }
    set filename [DataDir]/temp-[id process]
    set fid [open $filename "w"]
    chmod 0666 $filename
    # Hack to make person be an empty array: 
    set person(xyzzy) 1
    unset person(xyzzy)
    foreach i {treeopen lastgoodtimestamp closetimestamp checkinlist} {
        puts $fid [list set $i [set $i]]
    }
    foreach c $checkinlist {
        upvar #0 $c info
        foreach i [lsort [array names info]] {
            puts $fid [list set [set c]($i) $info($i)]
        }
        set person($info(person)) 1
    }
    close $fid
    Lock
    set filedest [DataDir]/batch-$batchid
    if {[file exists $filedest]} {
        unlink $filedest
    }
    frename $filename $filedest

    set fid [open $filename "w"]
    chmod 0666 $filename
    foreach i [lsort [array names person]] {
        puts $fid [EmailFromUsername $i]
    }
    puts $fid "bonsai-hookinterest@glacier"
    puts $fid "mcom.dev.client.build.busted"
    close $fid
    frename $filename [DataDir]/hooklist
    
    Unlock
}



proc ConstructMailTo {name subject} {
    return "<a href=\"mailto:$name?subject=$subject\">Send mail to $name</a>"
}



proc Log {str} {
    Lock
    set filename "[DataDir]/logfile"
    set fid [open $filename "a"]
    catch {chmod 0666 $filename}
    puts $fid "[fmtclock [getclock] "%D %H:%M"] $str"
    close $fid
    Unlock
}


proc GenerateProfileHTML {name} {
    global ldapserver ldapport
    foreach i {
        {cn Name}
        {mail E-mail}
        {telephonenumber Phone}
        {pager Pager}
        {nscpcurcontactinfo {Contact Info}}
    } {
        lassign $i n t
        lappend namelist $n
        set title($n) $t
        set value($n) ""
    }

    if {[catch {set fid [open "|./data/ldapsearch -b \"dc=netscape,dc=com\" -h $ldapserver -p $ldapport -s sub \"(mail=$name@netscape.com)\" $namelist" r]} errinfo]} {
        return "<B>Error -- Couldn't contact the directory server.</B><PRE>$errinfo</PRE>"
    }

    set result "<TABLE>"

    while {[gets $fid line] >= 0} {
        if {[regexp -- {^([a-z]*): (.*)$} $line foo n v]} {
            lappend value($n) $v
        }
    }

    if {[catch {close $fid} errinfo]} {
        return "<B>Error -- problem running ldapsearch.</B><PRE>$errinfo</PRE>"
    }

    foreach i $namelist {
        foreach v $value($i) {
            append result "<TR><TD align=right><B>$title($i):</B></TD>"
            append result "<TD>$v</TD></TR>"
        }
    }
    append result "</TABLE>"

    return $result
}


proc MyFmtClock {time} {
    return [fmtclock $time "%D %T"]
}


proc LoadMOTD {} {
    global motd
    Lock
    set motd {}
    if {[file exists [DataDir]/motd]} {
        uplevel #0 source [DataDir]/motd
    }
    Unlock
}

proc WriteMOTD {} {
    global motd
    Lock
    set fid [open [DataDir]/motd "w"]
    catch {chmod 0666 [DataDir]/motd}
    puts $fid [list set motd $motd]
    close $fid
    Unlock
}

proc LoadWhiteboard {} {
    global whiteboard origwhiteboard
    set whiteboard {}
    Lock
    if {[file exists [DataDir]/whiteboard]} {
        set whiteboard [read_file [DataDir]/whiteboard]
    }
    Unlock
    set origwhiteboard $whiteboard
}


proc WriteWhiteboard {} {
    global whiteboard origwhiteboard
    if {![cequal $origwhiteboard $whiteboard]} {
        Lock
        set filename "[DataDir]/whiteboard"
        if {[file exists $filename]} {
            catch {unlink $filename}
        }
        set fid [open $filename w]
        puts $fid $whiteboard
        close $fid
        catch {chmod 0666 $filename}
        Unlock
    }
}



proc LoadTreeConfig {} {
    global treelist treeinfo
    Lock
    set treelist {}
    catch {unset treeinfo}
    set filename data/configdata
    if {[file exists $filename]} {
        uplevel #0 source $filename
    }
    Unlock
}
        

proc Pluralize {str num} {
    if {$num == 1} {
        return $str
    } else {
        return "[set str]s"
    }
}


proc MailDiffs {name oldstr newstr} {
    if {[cequal $oldstr $newstr]} {
        return
    }
    set old "data/old[set name].[id process]"
    set new "data/new[set name].[id process]"
    set diffs data/diffs.[id process]
    set fid [open $old "w"]
    puts $fid $oldstr
    close $fid
    set fid [open $new "w"]
    puts $fid $newstr
    close $fid
    catch {exec diff -c -b $old $new > $diffs}
    set difftext [read_file $diffs]
    if {[clength $difftext] > 3} {
        set text "From: bonsai-daemon
To: bonsai-messageinterest@glacier, mcom.dev.client.build.busted
Subject: [SubjectTag] Changes made to $name
Mime-Version: 1.0
Content-Type: text/plain

$difftext
"

        exec /usr/lib/sendmail -t << $text
    }
    unlink $old
    unlink $new
    unlink $diffs    
}


proc PrettyDelta {delta} {
    set result ""
    set oneday [expr 24*60*60]
    if {$delta > $oneday} {
        set numdays [$delta / $oneday]
        append result " $numdays day"
        if {$numdays > 1} {
            append result "s"
        }
        set delta [expr $delta % $oneday]
    }
    set onehour [expr 60*60]
    set numhours [expr $delta / $onehour]
    if {$numhours > 0} {
        append result " $numhours hour"
        if {$numhours > 1} {
            append result "s"
        }
        set delta [expr $delta % $onehour]
    }
    set oneminute 60
    set numminutes [expr $delta / $oneminute]
    if {$numminutes > 0} {
        append result " $numminutes minute"
        if {$numminutes > 1} {
            append result "s"
        }
        set delta [expr $delta % $oneminute]
    }
    if {$delta > 0} {
        append result " $delta second"
        if {$delta > 1} {
            append result "s"
        }
    }
    return $result
}
    


# Generate a string to put at the head of a subject of an e-mail.
proc SubjectTag {} {
    global treeid
    if {[cequal $treeid default]} {
        return {[Bonsai]}
    } else {
        return "\[Bonsai-$treeid\]"
    }
}


# Confirm that the given password is right.  If not, generate HTML and exit.

proc CheckGlobalPassword {password {encoded {}}} {
    set fid [open data/passwd "r"]
    set correct [string trim [read $fid]]
    close $fid
    
    if {[clength $encoded] == 0} {
        set encoded [string trim [exec ./data/trapdoor $password]]
    }

    if {![cequal $correct $encoded]} {
        puts "<TITLE>Bzzzzt!</TITLE>"
        puts "<H1>Invalid password.</h1>"
        puts "Please click the <b>Back</b> button and try again."
        Log "Invalid admin password entered."
        exit
    }
}

proc CheckPassword {password} {
    set encoded [string trim [exec ./data/trapdoor $password]]
    set f [DataDir]/treepasswd
    set correct "xxx $encoded"
    if {[file exists $f]} {
        set fid [open $f "r"]
        set correct [string trim [read $fid]]
        close $fid
    }

    if {![cequal $correct $encoded]} {
        CheckGlobalPassword $password $encoded
    }
}


proc ParseTimeAndCheck {timestr} {
    if {[catch {set result [convertclock $timestr]}]} {
        puts "
<TITLE>Time trap</TITLE>
<H1>Can't grok the time</H1>
You entered a time of <tt>$timestr</tt>, and I can't understand it.  Please
hit <B>Back</B> and try again."
        exit
    }
    return $result
}



proc MungeTagName {name} {
    set result ""
    foreach c [split $name ""] {
        scan $c "%c" value
        if {$value <= 32 || [regexp {[ %/?*]} $c]} {
            append result [format "%%%02x" $value]
        } else {
            append result $c
        }
    }
    return $result
}


# Given a person's username, get back their full email address.
# ### This needs to be paramaterized...
proc EmailFromUsername {name} {
    regsub {%} $name {@} name
    if {[string first "@" $name] < 0} {
        append name "@netscape.com"
    }
    return $name
}
