# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

# Handle detection of system information from the UA string

package Litmus::UserAgentDetect;

use strict;

require Exporter;
use Litmus;
use Litmus::DB::Platform;
use Litmus::DB::Opsys;
use Litmus::DB::Branch;

our @ISA = qw(Exporter);
our @EXPORT = qw(detectBuildID);

# define some SQL queries we will use:
Litmus::DB::Platform->set_sql(detectplatform => qq{
                                                  SELECT p.platform_id
                                                  FROM platforms p, platform_products pp
                                                  WHERE 
                                                      ? REGEXP detect_regexp AND 
                                                      p.platform_id=pp.platform_id  AND
                                                      pp.product_id LIKE ?
                                                });
Litmus::DB::Branch->set_sql(detectbranch => qq{
                                                SELECT __ESSENTIAL__
                                                FROM __TABLE__
                                                WHERE 
                                                    ? REGEXP detect_regexp AND 
                                                    product_id LIKE ?
                                            });                                                

# constructor. Optionally you can pass a UA string 
# and it will be used. Otherwise the default is the 
# current useragent.
sub new {
    my $self = {};
    my $class = shift;
    my $ua = shift;
    
    bless($self);
    $self->{ua} = $main::ENV{"HTTP_USER_AGENT"};
    if ($ua) { $self->{ua} = $ua }
    return $self;
}

# default stringification is to return the ua:
use overload 
    '""' => \&ua;
    

sub ua {
    my $self = shift;
    
    # we pad the UA with a space since some of our regexp matches expect 
    # to match things at the end of the string. This is quite possibly 
    # a bug. 
    return $self->{ua}." ";
}

sub build_id {
    my $self = shift;
    my $ua = $self->{ua};
    
    # mozilla products only
    unless ($ua =~ /Mozilla\/5\.0/) {
        return undef;
    }
    $ua =~ /(200\d*)/;
    return $1; 
}

sub locale {
    my $self = shift;
    my $ua = $self->{ua};
    
    # mozilla products only
    unless ($ua =~ /Mozilla\/5\.0/) {
        return undef;
    }

    # Format (e.g.): 
    # Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8) Gecko/20051111 Firefox/1.5
    $ua =~ /Mozilla\/5\.0 \([^;]*; [^;]*; [^;]*; ([^;]*); [^;]*\)/;
    return $1; 
}

sub platform {
    my $self = shift;
    my $product = shift; # optionally, just lookup for one product
    
    if (! $product) { $product = '%' }
    
    my @platforms = Litmus::DB::Platform->search_detectplatform($self->ua, $product);
    return @platforms;
}

sub branch {
    my $self = shift;
    my $product = shift; # optionally, just lookup for one branch
    
    if (! $product) { $product = '%' }
    
    my @branches = Litmus::DB::Branch->search_detectbranch($self->ua, $product);
    return @branches;
}

# from the legacy API before we had an OO interface:
sub detectBuildId() {
    my $self = Litmus::UserAgentDetect->new($main::ENV{"HTTP_USER_AGENT"});
    
    return $self->build_id();
}

1;
