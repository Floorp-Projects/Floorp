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

package PLIF::DataSource::DebugStrings;
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
    # this is protocol agnostic stuff :-)
    if ($string eq 'debug.dumpVars') {
        return ('COSES', <<eof);
<!--
 !
 !  This example will dump every single string passed into it. For
 !  example, if you pass it a hash with one item 'data' containing two
 !  items 'a' and '(b)' with 'a' containing 'hello' and '(b)'
 !  containing an array of two values 'wonderful' and 'world', you
 !  would get as output the following (note that special characters
 !  '(' and ')' are automatically sanitised by COSES to '[' and ']'):
 !
 !     coses: last condition = 0
 !     coses: white space = 1
 !     data.a = hello
 !     data.[b].1 = wonderful
 !     data.[b].2 = world
 !
 !  This example uses almost all the features of COSES, and so is
 !  quite a useful example to study. (It doesn't use all of the values
 !  of <set>'s attributes nor the escaping attributes of <text>.) It's
 !  also a great help when debugging! You can use it at any point in a
 !  COSES document merely by nesting it, so you can, for example,
 !  study what is happening with a <set> statement. If you declare
 !  this example as having the name 'debug.dumpVars' then to embed it
 !  you would do:
 !
 !     <include href="debug.dumpVars"/>
 !
 !  This example is covered by the same license terms as COSES itself.
 !  Author: Ian Hickson
 !
 !-->
<text xmlns="http://bugzilla.mozilla.org/coses"
      xml:space="default"> <!-- trim whitespace -->
  <with variable="prefix">
    <if lvalue="((prefix))" condition="is" rvalue="scalar">
      <text value="  (prefix)"/> = <text value="((prefix))"/><br/>
    </if>
    <if lvalue="((prefix))" condition="is not" rvalue="scalar">
      <set variable="index" value="((prefix))" source="keys" order="case insensitive lexical">
        <if lvalue="(index)" condition="=~" rvalue="'[\.\(\)]">
          <!-- this can only be hit if COSES has been told to not 
               sanitise keys with special characters -->
          <text value="  (prefix).|(index)| is inaccessible"/><br/>
        </if>
        <else>
          <set variable="prefix" value="(prefix).(index)">
            <include href="debug.dumpVars"/>
          </set>
        </else>
      </set>
      <else>
        <text value="  (prefix)"/><br/>
      </else>
    </if>
  </with>
  <without variable="prefix">
    <set variable="prefix" value="()" source="keys" order="lexical">
      <include href="debug.dumpVars"/>
    </set>
  </without>
</text>
eof
    } else {
        return;
    }
}
