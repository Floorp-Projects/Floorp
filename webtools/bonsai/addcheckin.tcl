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

source globals.tcl

# cmdtrace on

assert {![info exists checkinlist]}

set inheader 1
set foundlogline 0

proc trim {str} {
    return [string trim $str]
}

set filelist {}
set log {}

set appendjunk {}
set repository {/m/src}


if {[cequal [lindex $argv 0] "-treeid"]} {
    lvarpop argv
    set forcetreeid [lvarpop argv]
}


# Stupid hack to make empty array
set group(xyzzy) 1
unset group(xyzzy)


for_file line $argv {
    set line [trim $line]
    if {$inheader} {
        switch -glob -- $line {
            {} {
                set inheader 0
            }
        }
    } elseif {!$foundlogline} {
        switch -glob -- $line {
            {?|*} {
                append appendjunk "$line\n"
                lassign [split $line "|"] chtype date name repository dir \
                    file version sticky branch addlines removelines
                set key [list $date $branch $repository $dir $name]
                lappend group($key) [list $file $version $addlines \
                                         $removelines $sticky]
            }
            {Tag|*} {
                set data [split $line "|"]
                lvarpop data
                set repository [lvarpop data]
                set tagtime [lvarpop data]
                set tagname [lvarpop data]
                regsub -all -- / $repository _ mungedname
                set filename "data/taginfo/$mungedname/[MungeTagName $tagname]"
                Lock
                if {[catch {set fid [open "$filename" "a"]}]} {
                    catch {mkdir data/taginfo}
                    catch {chmod 0777 data/taginfo}
                    catch {mkdir data/taginfo/$mungedname}
                    catch {chmod 0777 data/taginfo/$mungedname}
                    set fid [open "$filename" "a"]
                }
                puts $fid "$tagtime|[join $data {|}]"
                close $fid
                catch {chmod 0666 $filename}
                Unlock
            }
            {LOGCOMMENT} {
                set foundlogline 1
            }
        }
    } else {
        if {[cequal $line ":ENDLOGCOMMENT"]} {
            break
        }
        append log "$line\n"
    }
}

set plainlog $log
set log [html_quote [trim $log]]


Lock
LoadTreeConfig
regsub -all -- {[0-9][0-9][0-9][0-9][0-9]*} $log $BUGSYSTEMEXPR log
if {![info exists forcetreeid]} {
    regsub -all -- / $repository _ mungedname
    set filename "data/checkinlog$mungedname"
    set fid [open $filename "a"]
    catch {chmod 0666 $filename}
    puts $fid "[set appendjunk]LOGCOMMENT\n$plainlog\n:ENDLOGCOMMENT"
    close $fid
    
    ConnectToDatabase
    AddToDatabase $appendjunk $plainlog
    
}
Unlock


if {[info exists forcetreeid]} {
    set treestocheck $forcetreeid
} else {
    set treestocheck $treelist
}


foreach key [array names group] {
    lassign $key date branch repository dir name
    set branch2 [crange $branch 1 end]
    set files {}
    set fullinfo {}
    foreach i $group($key) {
        lassign $i file version addlines removelines
        lappend files $file
        lappend fullinfo $i
    }

    foreach treeid $treestocheck {
        if {[info exists treeinfo($treeid,nobonsai)]} {
            continue
        }

        if {![cequal $branch $treeinfo($treeid,branch)] && \
                ![cequal $branch2 $treeinfo($treeid,branch)]} {
            continue
        }

        if {![cequal $repository $treeinfo($treeid,repository)]} {
            continue
        }

        LoadDirList

        set okdir 0

        # Sigh.  We have some specific files listed as well as modules.  So,
        # painfully go through *every* file we're checking in, and see if any
        # of them are ones we're interested in.

        foreach f $files {
            set full $dir/$f
            foreach d $legaldirs {
                if {[string match $d $full]} {
                    set okdir 1
                    break
                }
            }
            if {$okdir} {
                break
            }
        }
        
        if {$okdir} {
            Lock
            catch {unset batchid}
            catch {unset checkinlist}
            LoadCheckins
            set id checkin-$date-[id process]
            lappend checkinlist $id
            upvar #0 $id foo
            set foo(person) $name
            set foo(date) $date
            set foo(dir) $dir
            set foo(files) $files
            set foo(log) $log
            set foo(treeopen) $treeopen
            set foo(fullinfo) $fullinfo
            WriteCheckins
            Log "Added checkin $name $dir $files"
            Unlock
            if {$treeopen} {
                set filename [DataDir]/openmessage
                foreach i $checkinlist {
                    upvar #0 $i info
                    if {[cequal $info(person) $name]} {
                        if {![cequal $id $i]} {
                            # This person already has a checkin, so we don't
                            # need to bother him again.
                            set filename thisFileDoesntExist
                        }
                    }
                }
            } else {
                set filename [DataDir]/closemessage
            }
            if {![info exists forcetreeid] && [file exists $filename]} {
                set fid [open $filename "r"]
                set text [read $fid]
                close $fid
                set profile [GenerateProfileHTML $name]
                set nextclose {[We don't remember close times any more...]}
                set name [EmailFromUsername $name]
                foreach k {name dir files log profile nextclose} {
                    regsub -all -- "%$k%" $text [set $k] text
                }
                exec /usr/lib/sendmail -t << $text
                Log "Mailed file $filename to $name"
            }
        }
    }
}


NOTDEF {

set tmp $log

while {[regexp -nocase -- {.*bugfix([ 0-9]*)(.*)$} $tmp foo list tmp]} {
    set fileversions {}
    foreach i [array names group] {
        lassign $i file version
        lappend $fileversions [list $file $version]
    }
    foreach b $list {
        set result [exec /usr/local/bin/lynx -dump "http://scopus.mcom.com/terrsplat/doclosebug.cgi?id=$b&directory=$dir&fileversions=$fileversions&who=$name&log=$plainlog"]
        if {$result != "OK\n"} {
        }
    }
}

}



exit
