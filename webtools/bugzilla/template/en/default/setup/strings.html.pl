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
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2007
# Everything Solved. All Rights Reserved.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

# This is just like strings.txt.pl, but for HTML templates (used by
# setup.cgi).

%strings = (
    footer => "</div></body></html>",

    # This is very simple. It doesn't support the skinning system.
    header => <<END_HTML
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
                      "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html>
<head>
    <title>Installation and Setup for Bugzilla ##bz_ver##</title>
    <link href="skins/standard/global.css" rel="stylesheet" type="text/css" />
</head>
<body id="bugzilla-installation">
    <h1>Installation and Setup for Bugzilla ##bz_ver##</h1>
    <div id="bugzilla-body">
    
        <p><strong>Perl Version</strong>: ##perl_ver##</p>
        <p><strong>OS</strong>: ##os_name## ##os_ver##</p>
END_HTML
,
);

1;

