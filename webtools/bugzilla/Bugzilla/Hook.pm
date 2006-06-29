# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>
#

package Bugzilla::Hook;

use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;

use strict;

sub process {
    my $name = shift;
    trick_taint($name);
    
    # get a list of all extensions
    my @extensions = glob(bz_locations()->{'extensionsdir'} . "/*");
    
    # check each extension to see if it uses the hook
    # if so, invoke the extension source file:
    foreach my $extension (@extensions) {
        # all of these variables come directly from code or directory names. 
        # If there's malicious data here, we have much bigger issues to 
        # worry about, so we can safely detaint them:
        trick_taint($extension);
        if (-e $extension.'/code/'.$name.'.pl') {
            do($extension.'/code/'.$name.'.pl');
            ThrowCodeError('extension_invalid', {
                name => $name, extension => $extension }) if $@;
        }
    }
    
}

1;

__END__

=head1 NAME

Bugzilla::Hook - Extendible extension hooks for Bugzilla code

=head1 SYNOPSIS

  use Bugzilla::Hook;

  Bugzilla::Hook::process("hookname");

=head1 DESCRIPTION

Bugzilla allows extension modules to drop in and add routines at 
arbitrary points in Bugzilla code. These points are refered to as 
hooks. When a piece of standard Bugzilla code wants to allow an extension
to perform additional functions, it uses Bugzilla::Hook's process() 
subroutine to invoke any extension code if installed. 

=over 4

=item C<process>

Invoke any code hooks with a matching name from any installed extensions. 
When this subroutine is called with hook name foo, Bugzilla will attempt 
to invoke any source files in C<bugzilla/extension/EXTENSION_NAME/code/foo.pl>. 
See C<customization.xml> in the Bugzilla Guide for more information on
Bugzilla's extension mechanism. 

=back
