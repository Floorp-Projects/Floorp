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

puts "Content-type: text/html

<HTML>"

CheckPassword $FORM(password)

Lock
LoadTreeConfig
Unlock

set repository $treeinfo($treeid,repository)
regsub -all -- / $repository _ mungedname

puts "<H1>Searching for tags in all files in $repository ...</h1>"
flush stdout

set numfiles 0
set numtags 0

my_for_recursive_glob filename $repository "*,v" {
    puts "$filename<br>"
    flush stdout
    set fid [open "|/tools/ns/bin/rlog -h $filename" r]
    set doingtags 0
    while {1} {
        if {[gets $fid line] < 0} {
            break
        }
        if {$doingtags} {
            if {![cequal "\t" [crange $line 0 0]]} {
                break
            }
            lassign [split [string trim $line] ":"] tag version
            if {[clength $tag] == 0 || [clength $version] == 0} {
                continue
            }
            set tag_[set tag]($cutename) [string trim $version]
        } elseif {[cequal [string trim $line] "symbolic names:"]} {
            set doingtags 1
            regsub -- {,v$} $filename {} tmp
            regsub -- {/([^/]*$)} $tmp {|\1} cutename
        }
    }
    catch {close $fid}
    incr numfiles
}

puts "<HR>"
flush stdout

set dir data/taginfo
catch {mkdir $dir}
catch {chmod 0777 $dir}
append dir /tmp_[set mungedname]_[id process]
catch {mkdir $dir}
catch {chmod 0777 $dir}

foreach n [lsort [info var tag_*]] {
    upvar #0 $n t
    set tagname [crange $n 4 end]
    puts "Dumping tag $tagname<br>"
    flush stdout
    set filename $dir/[MungeTagName $tagname]
    set fid [open "$filename" "w"]
    foreach f [lsort [array names t]] {
        puts $fid "0|add|$f|$t($f)"
    }
    close $fid
    incr numtags
}


Lock
set newdir data/taginfo/$mungedname
catch {exec rm -rf $newdir}
frename $dir $newdir
Unlock

puts "<HR><P>Done.  $numfiles files checked; $numtags tags created.<P>"

PutsTrailer
