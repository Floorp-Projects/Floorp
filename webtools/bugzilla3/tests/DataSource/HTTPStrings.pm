# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package DataSource::HTTPStrings;
use strict;
use vars qw(@ISA);
use PLIF::DataSource;
@ISA = qw(PLIF::DataSource);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dataSource.strings.default' or $class->SUPER::provides($service));
}

sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    if ($protocol ne 'http') {
        return; # ``we don't do CDs!!!''
    }
    if ($string eq 'hello') {
        return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>Hello <if lvalue="(data.session)" condition="ne" rvalue="">user #<text value="(data.session.userID)"/></if><else>World</else>!<br/><br/>visit nph-Main?command=Login to log in if you haven\'t already.<br/>At the moment to create a username and password, you have to ask Hixie to edit the database... :-/ (use open/sesame to test)<br/><br/><include href="debug.dumpVars"/><br/><br/><shrink source="(data.input.arguments)" target="shrunken"><include href="debug.dumpVars"/><br/><br/><expand source="(shrunken)" target="expanded"><include href="debug.dumpVars"/></expand></shrink><br/><br/><text escape="uri">Welcome To The URI World</text><br/><text escape="html">Welcome To The &lt;HTML&gt; World</text><br/><text escape="html"><text escape="uri"><text escape="xml">Welcome To &gt; The HTML+URI+XML+foo World</text></text></text></text>');
    } else {
        return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>Er. <include href="debug.dumpVars"/></text>');
    }
}
