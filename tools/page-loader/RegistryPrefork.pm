# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package ModPerl::RegistryPrefork;

# RegistryPrefork.pm originally from
# http://perl.apache.org/docs/2.0/user/porting/compat.html#Code_Porting
# backported for mod_perl <= 1.99_08

use strict;
use warnings FATAL => 'all';

our $VERSION = '0.01';

use base qw(ModPerl::Registry);

use File::Basename ();

use constant FILENAME => 1;

sub handler : method {
    my $class = (@_ >= 2) ? shift : __PACKAGE__;
    my $r = shift;
    return $class->new($r)->default_handler();
}

sub chdir_file {
    my $file = @_ == 2 ? $_[1] : $_[0]->[FILENAME];
    my $dir = File::Basename::dirname($file);
    chdir $dir or die "Can't chdir to $dir: $!";
}

1;
__END__
