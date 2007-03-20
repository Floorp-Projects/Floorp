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
    checking_dbd => '<tr><th colspan="4" class="mod_header">Database Modules</th></tr>',
    checking_optional => '<tr><th colspan="4" class="mod_header">Optional Modules</th></tr>',
    checking_modules  => <<END_HTML
<h2>Perl Modules</h2>

<div style="color: white; background-color: white">
    <p class="color_explanation">Rows that look <span class="mod_ok">like
    this</span> mean that you have a good version of that module installed.
    Rows that look <span class="mod_not_ok">like this</span> mean that you
    either don't have that module installed, or the version you have
    installed is too old.</p>
</div>
    
<table class="mod_requirements" border="1">
<tr>
    <th class="mod_name">Package</th>
    <th>Version Required</th> <th>Version Found</th>
    <th class="mod_ok">OK?</th>
</tr>
END_HTML
,

    footer => "</div></body></html>",

    # This is very simple. It doesn't support the skinning system.
    header => <<END_HTML
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" 
                      "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html>
<head>
    <title>Installation and Setup for Bugzilla ##bz_ver##</title>
    <link href="skins/standard/global.css" rel="stylesheet" type="text/css" />
    <link href="skins/standard/setup.css"  rel="stylesheet" type="text/css" />
</head>
<body id="bugzilla-installation">
    <h1>Installation and Setup for Bugzilla ##bz_ver##</h1>
    <div id="bugzilla-body">
        <p><strong>Perl Version</strong>: ##perl_ver##
          <strong>OS</strong>: ##os_name## ##os_ver##</p>
END_HTML
,
    module_details => <<END_HTML
<tr class="##row_class##">
    <td class="mod_name">##package##</td>
    <td class="mod_wanted">##wanted##</td>
    <td class="mod_found">##found##</td>
    <td class="mod_ok">##ok##</td>
</tr>
END_HTML
,
    module_found => '##ver##',
);

1;

