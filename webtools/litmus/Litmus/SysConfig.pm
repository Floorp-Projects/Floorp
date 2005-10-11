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
 # Portions created by the Initial Developer are Copyright (C) 2005
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::SysConfig;

use strict;

require Exporter;
use Litmus;
use Litmus::DB::Product;
use Litmus::Error;
use Litmus::Utils;
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
# optionally takes the product to configure for 
# and requires a url to load when done. this 
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
    
    my @products; 
    # if we already know the product, then just send it on:
    if ($product) {
        $products[0] = $product;
    } else {
        # we need to ask the user for the product then
        @products = Litmus::DB::Product->retrieve_all();
    }
    
    my $vars = {
        products   => \@products,
        ua         => Litmus::UserAgentDetect->new(),
        "goto" => $goto,
        cgidata   => $c,
        };
        
    # if the user already has a cookie set for this product, then 
    # load those values as defaults:
    if ($product && Litmus::SysConfig->getCookie($product)) {
        my $sysconfig = Litmus::SysConfig->getCookie($product);
        $vars->{"defaultplatform"} = $sysconfig->platform();
        $vars->{"defaultopsys"} = $sysconfig->opsys();
        $vars->{"defaultbranch"} = $sysconfig->branch();
    }
    
    my $cookie =  Litmus::Auth::getCookie();
    $vars->{"defaultemail"} = $cookie;
    $vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

    $vars->{"title"} = "Run Tests";
    
    Litmus->template()->process("runtests/sysconfig.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
}

# process a form containing sysconfig information. 
# takes a CGI object containing param data
sub processForm {
    my $self = shift;
    my $c = shift;
    
    my $product = Litmus::DB::Product->retrieve($c->param("product"));
    my $platform = Litmus::DB::Platform->retrieve($c->param("platform"));
    my $opsys = Litmus::DB::Opsys->retrieve($c->param("opsys"));
    my $branch = Litmus::DB::Branch->retrieve($c->param("branch"));
    my $buildid = $c->param("buildid");
    
    requireField("product", $product);
    requireField("platform", $platform);
    requireField("opsys", $opsys);
    requireField("branch", $branch);
    requireField("buildid", $buildid);
    
    # set a cookie with the user's testing details:
    my $prod =  Litmus::DB::Product->retrieve($c->param("product"));
    my $sysconfig = Litmus::SysConfig->new(
                            $product,
                            $platform, 
                            $opsys,
                            $branch,
                            $buildid
                            );
                                    
    return $sysconfig;
}
1;
