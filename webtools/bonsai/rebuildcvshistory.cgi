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
source myglobrecur.tcl

if {[llength $argv] == 5} {
    lassign $argv treeid FORM(startfrom) FORM(firstfile) FORM(subdir) FORM(modules)
} else {
    puts "Content-type: text/plain

<HTML>"

    CheckPassword $FORM(password)
}

set startfrom [ParseTimeAndCheck [FormData startfrom]]

set firstfile [string trim [FormData firstfile]]

set subdir [string trim [FormData subdir]]

Lock
LoadTreeConfig
Unlock

ConnectToDatabase

set repository $treeinfo($treeid,repository)
regsub -all -- / $repository _ mungedname

puts "Rebuilding entire checkin history in $treeinfo($treeid,description) ..."
flush stdout



# cmdtrace on

set repositoryid [GetId repositories repository $repository]

proc ProcessOneFile {filename} {
    global repository startfrom rlogcommand

    puts "$filename"
    flush stdout
    set fid [open "|$rlogcommand $filename" r]
    set doingtags 0
    catch {unset branchname}
    regsub -- {,v$} $filename {} filerealname
    set filehead [file dirname $filerealname]
    regsub -- "^$repository" $filehead {} filehead
    regsub -- {^/} $filehead {} filehead
    if {[clength $filehead] == 0} {
        set filehead "."
    }
    set filetail [file tail $filerealname]
    while {1} {
        if {[gets $fid line] < 0} {
            break
        }
        set trimmed [string trim $line]
        if {$doingtags} {
            if {![cequal "\t" [crange $line 0 0]]} {
                set doingtags 0
            } else {
                lassign [split $trimmed ":"] tag version
                if {[clength $tag] == 0 || [clength $version] == 0} {
                    continue
                }
                set version [string trim $version]

                set branchid [GetId branches branch $tag]
                set dirid [GetId dirs dir $filehead]
                set fileid [GetId files file $filetail]
# Don't touch the tags database for now.  Nothing uses it, and it just takes
# up too much damn space.
#                SendSQL "replace into tags (branchid, repositoryid, dirid, fileid, revision) values ($branchid, $repositoryid, $dirid, $fileid, '$version')"

                set vlist [split $version '.']
                set sub [expr [llength $vlist] - 2]
                if {[cequal "0" [lindex $vlist $sub]]} {
                    # Aha!  Second-to-last being a zero is CVS's special way
                    # of remembering a branch tag.
                    set bnum [join [lreplace $vlist $sub $sub] "."]
                    set branchname($bnum) $tag
                }
                continue
            }
        }
        switch -regexp -- $line {
            {^symbolic names}  {
                set doingtags 1
            }
            {^revision ([0-9.]*)$} {
                set indesc 0
                while {1} {
                    if {$indesc} {
                        if {[cequal $line "----------------------------"] ||
                            [cequal $line "============================================================================="]} {
                            # OK, we're done.  Write it out.
                            if {[info exists revision] &&
                                [info exists datestr] &&
                                [info exists author]} {
                                if {[regexp -- {^([0-9]*)/([0-9]*)/([0-9]*) ([0-9]*):([0-9]*):([0-9]*)$} $datestr foo year month day hours mins secs]} {
                                    set date [convertclock "$month/$day/$year $hours:$mins:$secs" GMT]
                                    
                                    if {$date >= $startfrom} {
                                        set tbranch "T$branch"
                                        if {[cequal $tbranch "T"]} {
                                            set tbranch ""
                                        }
                                        set entrystr "C|$date|$author|$repository|$filehead|$filetail|$revision||$branch|+$pluscount|-$minuscount"
                                        AddToDatabase $entrystr $desc
                                    }
                                }
                            }
                            set indesc 0
                        } else {
                            append desc $line
                            append desc "\n"
                        }
                    } else {
                        switch -regexp -- $line {
                            {^revision ([0-9.]*)$} {                        
                                if {[regexp -- {^revision ([0-9.]*)$} $line foo new]} {
                                    set revision $new
                                    catch {unset datestr}
                                    catch {unset author}
                                    set pluscount 0
                                    set minuscount 0
                                    set desc {}

                                    regsub -- {.[0-9]*$} $revision {} bnum
                                    if {[info exists branchname($bnum)]} {
                                        set branch "$branchname($bnum)"
                                    } else {
                                        set branch ""
                                    }
                                }
                            }
                            {^date:} {
                                regexp -- {^date: ([0-9 /:]*);  author: ([^;]*);} $line foo datestr author
                                regexp -- {lines: \+([0-9]*) -([0-9]*)} $line foo pluscount minuscount
                            }
                            {^branches: [0-9 .;]*$} {
                                # Ignore these lines; make sure they don't
                                # become part of the desciption.
                            }
                            default {
                                set indesc 1
                                set desc "$line\n"
                                
                            }
                        }
                    }
                    if {[gets $fid line] < 0} {
                        break
                    }
                }
            }
        }
    }
    catch {close $fid}
}


proc ProcessDirectory {dir} {
    global firstfile
    my_for_recursive_glob filename $dir "*,v" {
        if {![cequal $firstfile ""]} {
            if {![cequal $filename $firstfile]} {
                puts "Skipping $filename"
                flush stdout
                continue
            }
            set firstfile ""
        }
        ProcessOneFile $filename
    }
}



proc digest {str} {
    global array
    set key [lvarpop str]
    if {[cequal [cindex [lindex $str 0] 0] "-"]} {
        lvarpop str
    }
    set array($key) $str
}



set env(CVSROOT) $treeinfo($treeid,repository)
set origdir [pwd]
cd /
set fid [open "|$cvscommand checkout -c" r]
cd $origdir

set curline ""
while {[gets $fid line] >= 0} {
    if {[ctype space [cindex $line 0]]} {
        append curline $line
    } else {
        digest $curline
        set curline $line
    }
}
digest $curline
close $fid


set startingdir $repository/$subdir

regsub -- {/\.$} $startingdir {} startingdir
regsub -- {/$} $startingdir {} startingdir


set oldlist {}

set list {}

if {[info exists FORM(modules)]} {
    set list [split $FORM(modules) ","]
}

if {[lempty $list]} {
    set list $treeinfo($treeid,module)
}


while {![cequal $list $oldlist]} {
    set oldlist $list
    set list {}
    foreach i $oldlist {
        if {[info exists array($i)]} {
            set list [concat $list $array($i)]
            # Do an unset to prevent infinite recursion.
            unset array($i)
        } else {
            lappend list $i
        }
    }
}


set tlist {}
catch {unset present}
foreach i $list {
    if {![info exists present($i)]} {
        lappend tlist $i
        set present($i) 1
    }
}
catch {unset present}


set list {}

foreach i $tlist {
    set d $repository/$i
    regsub -- {/\.$} $d {} d
    regsub -- {/$} $d {} d
    lappend list $d
}

if {[lempty $list]} {
    set $list $startingdir
}

set slen [expr [clength $startingdir] - 1]

puts "Doing directories: $list"

foreach dir $list {
    if {![cequal [crange $dir 0 $slen] $startingdir]} {
        puts "*** Skipping $dir ***"
        continue
    }
    if {![file isdirectory $dir]} {
        if {[file isfile $dir]} {
            ProcessOneFile $dir
        }
    } else {
        ProcessDirectory $dir
    }
}




# puts "<HR>Putting entries ($count unique descriptions) into database...<P>"
# flush stdout

# set infid [open data/checkinlog$mungedname "r"]

# ConnectToDatabase

# set buffer {}
# set desc {}
# set indesc 0
# set done 0
# while {[gets $infid line] >= 0} {
#     if {$indesc} {
#       if {[cequal $line ":ENDLOGCOMMENT"]} {
#           AddToDatabase $buffer $desc
#           set buffer {}
#           set desc {}
#           set indesc 0
#           incr done
#           if {$done % 5 == 0} {
#               puts "$done done.<BR>"
#               flush stdout
#           }
#       } else {
#           append desc $line
#           append desc "\n"
#       }
#     } else {
#       if {[cequal $line "LOGCOMMENT"]} {
#           set indesc 1
#       } else {
#           append buffer $line
#           append buffer "\n"
#       }
#     }
# }
# close $infid


        



# puts "<HR>"
# flush stdout

# set dir data/taginfo
# catch {mkdir $dir}
# catch {chmod 0777 $dir}
# append dir /tmp_[set mungedname]_[id process]
# catch {mkdir $dir}
# catch {chmod 0777 $dir}

# set numtags 0

# foreach n [lsort [info var tag_*]] {
#     upvar #0 $n t
#     set tagname [crange $n 4 end]
#     puts "Dumping tag $tagname<br>"
#     flush stdout
#     set filename $dir/[MungeTagName $tagname]
#     set fid [open "$filename" "w"]
#     foreach f [lsort [array names t]] {
#         puts $fid "0|add|$f|$t($f)"
#     }
#     close $fid
#     incr numtags
# }


# Lock
# set newdir data/taginfo/$mungedname
# catch {exec rm -rf $newdir}
# frename $dir $newdir
# Unlock

# puts "<HR><P>Done.  $numfiles files checked; $numtags tags created.<P>"

