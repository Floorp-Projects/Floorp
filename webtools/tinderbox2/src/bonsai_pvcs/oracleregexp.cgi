#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 



print "Content-type: text/html\n\n";

$out =<<EOF
<HTML>
<HEAD>
   <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=iso-8859-1">
   <META NAME="Author" CONTENT="lloyd tabb">
   <META NAME="GENERATOR" CONTENT="Mozilla/4.0 [en] (WinNT; I) [Netscape]">
   <TITLE>Regular expressions in the cvs_pvcs query tool</TITLE>
</HEAD>
<BODY>

<H1>
Description of Oracle regular expression syntax.</H1>
Regular expressions are a powerful way of specifying complex searches.


<h2>LIKE</h2>
<p>
The LIKE predicate provides the only pattern matching capability in SQL for the character data types. It takes the following form
</p>
<pre>
columnname [NOT] LIKE pattern-to-match
</pre>
<p>
The pattern match characters are the percent sign (%) to denote 0 or more arbitrary characters, and the underscore (_) to denote exactly one arbitrary character.
</p>
<p>

List the employee numbers and surnames of all employees who have a surname beginning with C.
</p>
<pre>
SELECT empno,surname
FROM employee
WHERE surname LIKE 'C%'
</pre>
<p>
List all course numbers and names for any course to do with accounting.
</p>
<pre>
SELECT courseno,cname
FROM course
WHERE cname LIKE '%ccount%'
</pre>

<p>
List all employees who have r as the second letter of their forename.
</p>
<pre>
SELECT surname, forenames
FROM employee
WHERE forenames LIKE '_r%'
</pre>

&nbsp;
</BODY>
</HTML>
EOF
;

print $out;
exit 0;
