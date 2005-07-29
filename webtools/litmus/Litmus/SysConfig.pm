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

package Litmus::SysConfig;

use strict;

require Exporter;
use Litmus;
use Litmus::DB::Product;
use Litmus::Error;
use CGI;

our @ISA = qw(Exporter);
our @EXPORT = qw();

my $configcookiename = $Litmus::Config::sysconfig_cookiename;

sub new {
    my ($class, $product, $platform, $opsys, $branch, $buildid) = @_;

    my $self = {};
    bless($self);
    
    $self->{"product"}  = $product;
    $self->{"platform"} = $platform;
    $self->{"opsys"}    = $opsys;
    $self->{"branch"}    = $branch;
    $self->{"buildid"}  = $buildid;
    
    return $self;
}

sub setCookie {
    my $self = shift;
    
    my %cookiedata;
    
    my $c = new CGI;
    my $cookie = $c->cookie( 
        -name   => $configcookiename.'_'.$self->{"product"}->productid(),
        -value  => join('|', $self->{"product"}->productid(), $self->{"platform"}->platformid(),
             $self->{"opsys"}->opsysid(), $self->{"branch"}->branchid(), $self->{"buildid"}),
        -domain => $main::ENV{"HTTP_HOST"},
    );
    
    return $cookie;
}

sub getCookie() {
    my $self = shift;
    my $c = new CGI;
    my $product = shift;
    
    my $cookie = $c->cookie($configcookiename.'_'.$product->productid());
    if (! $cookie) {
        return;
    }
    
    my @sysconfig = split(/\|/, $cookie);
    
    return new(undef,
               Litmus::DB::Product->retrieve($sysconfig[0]), 
               Litmus::DB::Platform->retrieve($sysconfig[1]), 
               Litmus::DB::Opsys->retrieve($sysconfig[2]), 
               Litmus::DB::Branch->retrieve($sysconfig[3]), 
               $sysconfig[4],
               );    
}

sub product() {
    my $self = shift;
    
    return $self->{"product"};
}

sub platform() {
    my $self = shift;
    
    return $self->{"platform"};
}

sub opsys() {
    my $self = shift;
    
    return $self->{"opsys"};
}

sub branch() {
    my $self = shift;
    
    return $self->{"branch"};
}

sub buildid() {
    my $self = shift;
    
    return $self->{"buildid"};
}

# display the system configuration form
# takes the product to configure for 
# and a url to load when done. this 
# url should call processForm() to 
# set the sysconfig cookie and do 
# error handling.
# optionaly, pass a CGI object and its
# data will be stored in the form so 
# the receiving script can access it.
sub displayForm {
    my $self = shift;
    my $product = shift;
    my $goto = shift;
    my $c = shift; 
    
    # get all possible platforms for that product
    my @platforms = Litmus::DB::Platform->search(product => $product);
    
    # get each possible OS per platform:
    my %opsyses;
    foreach my $curplatform (@platforms) {
        my @opsyses = Litmus::DB::Opsys->search(platform => $curplatform);
        $opsyses{$curplatform->platformid()} = \@opsyses;
    }
    
    # get all possible branches for the product
    my @branches = Litmus::DB::Branch->search(product => $product);
    
    my $vars = {
        product   => $product,
        platforms => \@platforms,
        opsyses   => \%opsyses,
        branches  => \@branches,
        ua        => Litmus::UserAgentDetect->new(),
        defaultplatform => Litmus::DB::Platform->autodetect($product),
        "goto" => $goto,
        cgidata   => $c,
        };
        
    # if the user already has a cookie set for this product, then 
    # load those values as defaults:
    my $sysconfig = Litmus::SysConfig->getCookie($product);
    if ($sysconfig && $sysconfig->product() == $product->productid()) {
        $vars->{"defaultplatform"} = $sysconfig->platform();
        $vars->{"defaultopsys"} = $sysconfig->opsys();
        $vars->{"defaultbranch"} = $sysconfig->branch();
    }
    
    $vars->{"defaultemail"} = Litmus::Auth::getCookie();
    
    Litmus->template()->process("runtests/sysconfig.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
}

# process a form containing sysconfig information. 
# takes a CGI object containing parma data
sub processForm {
    my $self = shift;
    my $c = shift;
    
    # the form has an opsys select box for each possible platform, named 
    # opsys_n where n is the platform id number. We get the correct 
    # opsys select based on whatever platform the user selected: 
    my $opsys = $c->param("opsys_".$c->param("platform"));
    
    # set a cookie with the user's testing details:
    warn $c->param("product");
    my $prod =  Litmus::DB::Product->retrieve($c->param("product"));
    my $sysconfig = Litmus::SysConfig->new(
                            $prod,
                            Litmus::DB::Platform->retrieve($c->param("platform")), 
                            Litmus::DB::Opsys->retrieve($opsys),
                            Litmus::DB::Branch->retrieve($c->param("branch")),
                            $c->param("buildid"),
                            );
                                    
    return $sysconfig;
}
1;