# Example configuration file for Bonsai

# The Bonsai modules and their relation to cvs

# @::TreeList is a list of all configured Bonsai modules
# to add a module, add its name to @::TreeList
# then duplicate the "default" entry in @::TreeInfo and
# change the values appropriately

@::TreeList = ('default');

%::TreeInfo = (
     default => {
          branch       => '',
          description  => 'All',
          module       => 'all',
          repository   => '/usr/local/cluster1',
          shortdesc    => 'All Files in the Repository',
                },
	       );

1;
