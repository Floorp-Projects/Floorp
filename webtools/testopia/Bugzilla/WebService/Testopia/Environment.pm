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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Dallas Harken <dharken@novell.com>

package Bugzilla::WebService::Testopia::Environment;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("current_tab", "environment");
    
    foreach (keys(%$query))
    {
        $cgi->param($_, $$query{$_});
    }
        
    my $search = Bugzilla::Testopia::Search->new($cgi);

    # Result is an array of environment hash maps 
    return Bugzilla::Testopia::Table->new('environment', 
                                          'tr_xmlrpc.cgi', 
                                          $cgi, 
                                          undef,
                                          $search->query()
                                          )->list();
}

sub get
{
    my $self = shift;
    my ($environment_id) = @_;

    $self->login;    

    #Result is a environment hash map
    my $environment = new Bugzilla::Testopia::Environment($environment_id);

    if (not defined $environment)
    {
        $self->logout;
        die "Environment, " . $environment_id . ", not found"; 
    }
    
    if (not $environment->canview)
    {
        $self->logout;
        die "User Not Authorized";
    }
    
    $self->logout;

    return $environment;
}

sub list
{
    my $self = shift;
    my ($query) = @_;

    $self->login;
   
    my $list = _list($query);
    
    $self->logout;
    
    return $list;    
}

sub create
{
    my $self =shift;
    my ($new_values) = @_;

    $self->login;

    my $environment = new Bugzilla::Testopia::Environment($new_values);
    
    my $result = $environment->store(); 
    
    $self->logout;
    
    # Result is new environment id
    return $result;
}

sub update
{
    my $self =shift;
    my ($environment_id, $new_values) = @_;

    $self->login;

    my $environment = new Bugzilla::Testopia::Environment($environment_id);
    
    if (not defined $environment)
    {
        $self->logout;
        die "Environment, " . $environment_id . ", not found"; 
    }
    
    if (not $environment->canedit)
    {
        $self->logout;
        die "User Not Authorized";
    }

    my $result = $environment->update($new_values);

    $environment = new Bugzilla::Testopia::Environment($environment_id);
    
    $self->logout;

    # Result is modified environment, otherwise an exception will be thrown
    return $environment;
}

sub get_runs
{
    my $self = shift;
    my ($environment_id) = @_;

    $self->login;

    my $environment = new Bugzilla::Testopia::Environment($environment_id);

    if (not defined $environment)
    {
        $self->logout;
        die "Environment, " . $environment_id . ", not found"; 
    }
    
    if (not $environment->canview)
    {
        $self->logout;
        die "User Not Authorized";
    }
    
    my $result = $environment->runs();
    
    $self->logout;

    # Result is list of test runs for the given environment
    return $result;
}

1;