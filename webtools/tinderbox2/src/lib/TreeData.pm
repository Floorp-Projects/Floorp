# -*- Mode: perl; indent-tabs-mode: nil -*-

# TreeData.pm - the configuration file which describes the local VC
# repository data.  Only TinderDB::VC and VCDisplay know the
# datastructure contained in this file.  Other modules can only check
# if a string is a valid tree name or can get the list of all valid
# trees through funtional interfaces.  Also this file store the list
# of valid tree states.  The TreeState is used as an informational
# message only it does not directly effect the operation of the
# version control system. The TreeState is manipulated via the
# TinderHeader interface.

# This data used to be configurable via the treeadmin interface but we
# can not longer support that as the format of this data depends on
# the version control system in use and abstraction prevents
# treeadmin.cgi from knowing those details.  It is safer to have this
# data stored in VC in its own file, in case of problems. So many
# modules need to know if a tree name is valid it would be hard for a
# CGI script to pass that data back to all the modules.


# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:13:59 $ 
# $Author: mcafee%netscape.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Attic/TreeData.pm,v $ 
# $Name:  $ 


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 




package TreeData;

$VERSION = '#tinder_version#';


# Each tree is a map from a name to all the information necessary to
# do a checkout of the sources (in this case cvs repository to
# checkout, module, branch needed).  Tree names are used as parts of
# filenames so should not contain ' ' or '/' or unprintable
# characters.  You may need to change this datastructure if you do not
# use CVS.

# Who uses tree data?  

# Only the VCDisplay and TinderDB::VC know what the internal structure
# of the tree hash is.  All other modules only need to use the name of
# a valid tree.

# Also the client side build script needs to know how to checkout a
# tree.

# dir_pattern is a hack to get arround the fact that CVS does not
# always understand the module structure.  If we can discribe what is
# in the module using a perl pattern we can implement lost
# CVS functionality in perl.

# It may interest you to know we are currently using this format to
# build sets of locally developed libraries.  Each tree is really a
# set of CVS modules which need to be built in some order due to
# dependencies on the modules.  The build script knows how to do this.
# As far as tinderbox is concerned the tree is a single CVS module
# which checks out the whole set of modules.  The rest of development
# thinks of a tree as a set of modules.



%VC_TREE = (
#	    SeaMonkey => {
#			  root => ':pserver:anonymous@'.
#			  	  'cvs-mirror.mozilla.org:'.
#                                  '/cvsroot',
#
#                          root => '/devel/java_repository',
#
#			  module => 'SeaMonkeyAll',
#			  branch => 'main',
#                          dir_pattern => '^/mozilla',
#			 },

	    # these are dummy trees for testing.	    
            
	    'Project_A' =>  {
                   root => '/devel/java_repository',
                   module => 'SeaMonkeyAll',
                   branch => 'main',
                  },
	    'Project_B' =>  {
                   root => '/devel/java_repository',
                   module => 'SeaMonkeyAll',
                   branch => 'main',
                  },
            
	    'Project_C' =>  {
                   root => '/devel/java_repository',
                   module => 'SeaMonkeyAll',
                   branch => 'main',
                  },
	   );


# We group trees into sets so that individual managers can get a page
# of all the projects they manage.  This is a mapping from managers
# email address to the set of projects they are interested in.  This
# is only used by the Summaries.pm module and the static portion can
# be left blank if you do not wish for these pages to be generated.


%VC_TREE_GROUPS = (
                   'jim' => {
                             'Project_A' => 1, 
                             'Project_B' => 1,
                            },
                   'fred' => {
                              'Project_C' => 1,
                             },
                  );

# We always want there to be one summary pages showing all trees.

foreach $tree (keys %VC_TREE) {
  $VC_TREE_GROUPS{'all_trees'}{$tree} =1;
}


# This hash holds the colors used for the cells in the VC column of
# the build table.  The keys of this hash are the definative list of
# tree states.  The code makes no assumptions as to how many states
# there are or what they mean, it only displays the history of the
# state.

%TREE_STATE2COLOR = (
		     'Open' => "white",
		     'Restricted' => "#e7e7e7", # a light grey
		     'Closed' => "silver",
		    );


sub TreeState2color {
  my ($state) = @_;

  return $TREE_STATE2COLOR{$state};
}


sub get_all_tree_states {
  my @tree_states;

  @tree_states = sort keys %TREE_STATE2COLOR;
  
  return @tree_states;
}



sub tree_exists {
  my ($tree) = @_;

  ($VC_TREE{$tree}) &&
    return 1;

  return 0;
}


sub get_all_trees {
  my (@out);

  @out = sort (keys %VC_TREE);

  return @out;

}

