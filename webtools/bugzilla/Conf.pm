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
# The Original Code is the Bugzilla Installer.
#
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are
# Copyright (C) 2002 Zach Lipton.  All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable
# instead of those above.  If you wish to allow use of your
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

package Conf;
no warnings; # evil beings...

use Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(ask holduntilkey output setConf getConf getParam setParam);

# this is very important. We set a default controller which is responsible 
# for getting data from the user and returning it back to us. Our default 
# is rather simple, but if a packager wishes to use a Tk front-end or customize 
# the look and feel of the installation process to match with their system, they 
# can override our controller by making a new one and setting 
# $Conf::controller = "foo" in a custom Configure.pl
# This is really intended for debian-style configuration systems
$controller = "Conf::DefaultController"; 


unless (eval("use $controller")) {
	die "ack, something went wrong when I tried to use controller $controller $@";
}

eval("use Conf::Supplies::Config"); # do this in an eval because it may not exist yet
# this module will store defaults for the installation

# ASK:
# call me like this:
# ask('questionname','question?','default answer');
sub ask {
    my ($name, $question, $default) = @_;
    if ($Conf::Supplies::Config::answers{$name}) { # if it's in Config.pm, use it
        $default = $Conf::Supplies::Config::answers{$name};
    }
    _ask($name,$question,$default);
    if ($answer eq "") { $answer = $default; } # handle the default
    $main::c{$name} = $answer;
} 

#HOLDUNTILKEY
# allows you to pause until the user presses a key
sub holduntilkey {
	_holduntilkey();
}

sub output {
    my ($output, $loud) = @_;
    _output($output,$loud); 
}

sub setConf($$) {
    my ($name, $value) = @_;
    $main::c{$name} = $value; # and set it
}


sub getConf($) { 
    my $name = @_;
    return $main::c{$name}; # return the value
}

# ask for the param name
sub getParam($) {
    my $param = @_;
    return $params{$param}; # return the param
}

# set a param
sub setParam($$) {
    my ($name, $value) = @_;
    $params{$name} = $value; # and set it
}

1;
