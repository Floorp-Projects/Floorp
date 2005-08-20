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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

package Litmus::DB::Format;

use strict;
use base 'Litmus::DBI';

use IO::File;
use Litmus::Error;
use Litmus;

Litmus::DB::Format->table('test_format_lookup');

Litmus::DB::Format->columns(All => qw/format_id name/);

Litmus::DB::Format->column_alias("format_id", "formatid");

Litmus::DB::Format->has_many(tests => "Litmus::DB::Test");

# WARNING: THIS RELIES ON AN UNDOCUMENTED API IN CLASS::DBI (_init)
# we do some dark magic here to load in the format file 
# when we get created:
sub _init {
    my $self = shift;
    $self = $self->SUPER::_init(@_);
    
    $self->loadFormatFile();
    
    return $self;
}

# here's where the real magic happens. Each format file has a %column_mappings
# hash that maps a format column (i.e. something like steps or expectedresults 
# or something similarly useful to human beings) to a database column (i.e. 
# t1 and friends). Give this sub a format column name, and you'll get the 
# database column name back. 
sub getColumnMapping {
    my $self = shift;
    my $colname = shift;
    
    return $self->{LITMUS_column_mappings}{$colname}; 
}

# return a list of all fields defined by the format
sub fields {
    my $self = shift;
    return keys(%{$self->{LITMUS_column_mappings}});
}
    
sub loadFormatFile {
    my $self = shift;
    
    unless ($self->name()) {
        internalError("Invalid or non-existant format found");
    }
    
    my $formatdir = "formats/".$self->name()."/";
    my $formatfile = $formatdir.$self->name().".pl";
    
    unless (-e $formatfile) {
        internalError("Could not locate format file for format ". $self->formatid());
    }
    
    our %column_mappings;
    
    do($formatfile);
    internalError("couldn't parse format file $formatfile: $@") if $@;
    internalError("couldn't execute format file $formatfile: $!") if $!;
    
    # store the column mapping table and formatdir for later use
    $self->{LITMUS_column_mappings} = \%column_mappings;
    $self->{LITMUS_format_dir} = $formatdir;

}

# this gets called from templates when we want to actually display the data 
# held by a format. We're given the test object and the name of the 
# format template to display. The template we process gets a "test" variable
# with the testcase to display data for. 
sub display {
    my $self = shift;
    my $test = shift; # test object
    my $showedit = shift;  # show test editing UI?
    my $template = shift; # the format template to process
    
    # we need a new Template object here or we collide with ourself:
    my $t = Litmus::Template->create();
    
    my $vars = {
        test => $test,
        showedit => $showedit,
    };
    
    # the template file to process itself:
    my $templateloc = $self->{LITMUS_format_dir}.$template;
    
    # since the format template is outside the include path, we
    # use IO::File to open a filehandle and hand it to process()
    my $fh = new IO::File;
    $fh->open("< ".$templateloc) ||
        internalError("could not open format template $templateloc");
    
    my $output = ''; # store the output and pass it to our caller
    
    $t->process($fh, $vars, \$output) || 
        internalError("could not process format template $templateloc");
    
    return $output; 
}

1;