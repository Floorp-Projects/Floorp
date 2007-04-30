#
# Bootstrap utility functions.
# 

package Bootstrap::Util;

use base qw(Exporter);

our @EXPORT_OK = qw(CvsCatfile);

use strict;

##
# Turn an array of directory/filenames into a CVS module path.
# ( name comes from File::Spec::catfile() )
#
# Note that this function does not take any arguments, to make the usage
# more like File::Spec::catfile()
##
sub CvsCatfile {
   return join('/', @_);
}

1;
