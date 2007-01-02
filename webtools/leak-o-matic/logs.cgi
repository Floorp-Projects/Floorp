#!/usr/bin/perl -w
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
# The Original Code is Mozilla Leak-o-Matic.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corp.  Portions created by Netscape Communucations
# Corp. are Copyright (C) 1999 Netscape Communications Corp.  All
# Rights Reserved.
# 
# Contributor(s):
# Chris Waterson <waterson@netscape.com>
#
# $Id: logs.cgi,v 1.7 2007/01/02 22:54:24 timeless%mozdev.org Exp $
#

#
# ``Front door'' script that shows all of the logs that are
# available for perusal
#

use 5.006;
use strict;
use CGI;
use POSIX;

$::query = new CGI();
# If you want to support specifying a different directory
# you will need to change this code:
#$::logdir = $::query->param('logdir');
$::logdir = 'data' unless $::logdir; # default is 'data' subdir

print $::query->header;
print $::query->start_html("Leak-o-Matic");

print qq{
<table border='0' cellpadding='0' cellspacing='0' width='100%'>
  <tr>
    <td bgcolor='#000000' valign='top'>
      <img src='http://www.mozilla.org/images/mozilla-banner.gif'
           border="0" alt='mozilla.org' href='http://www.mozilla.org/'>
    </td>
  </tr>
</table>

<center>
<h1>Leak-o-Matic</h1>
</center>

<p>
Welcome to the Marvelous Leak-o-Matic. Listed below are the leak logs
that are currently available for your perusal.
</p>

<div align='center'>
<table border='0' cellpadding='2' cellspacing='0' bgcolor='#EEEEEE'>
<tr><td bgcolor='#DDDDDD' align='center' colspan='3'><b>Leak-o-Matic Logs</b></td></tr>

};

# ``ls'' the directory, ``-1r'' returns it sorted by name, reversed, so
# the most recent logs will be at the top.

ZIP: foreach (qx/ls -1r $::logdir\/*.zip/) {
    chomp;

    next ZIP unless (/(\d\d\d\d)(\d\d)(\d\d)/);

    my ($year, $month, $day) = ($1, $2, $3);

    print "<tr>\n";
    print "<td>$month/$day/$year</td>\n";
    print "<td><a href='leaks.cgi?log=$_'>Leaks</a></td>\n";
    print "<td><a href='bloat-log.cgi?log=$_'>Bloat Log</a></td>\n";
    print "</tr>\n";
}

print qq{
</table>
</div>

<p>
For more information on how to use the Leak-o-Matic, see the

<a href="instructions.html">instructions</a>.
</p>

};

print '<small>$Id: logs.cgi,v 1.7 2007/01/02 22:54:24 timeless%mozdev.org Exp $</small>';
print $::query->end_html;

