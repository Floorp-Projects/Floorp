# -*- Mode: perl; indent-tabs-mode: nil -*-

# TreeData.pm - the configuration file which describes the local VC
# repository data.  Only TinderDB::VC and VCDisplay know the
# datastructure contained in this file.  Other modules can only check
# if a string is a valid tree name or can get the list of all valid
# trees through functional interfaces.  Also this file store the list
# of valid tree states.  The TreeState is used as an informational
# message only it does not directly effect the operation of the
# version control system. The TreeState is manipulated via the
# TinderHeader interface.

# This data used to be configurable via the treeadmin interface but we
# do not support that. The format of this data depends on the version
# control system in use and abstraction prevents treeadmin.cgi from
# knowing those details.  I may code a TreeData specific admin-CGI
# script in the future but I do not have time now and the benefits are
# not so clear.  For bonsai users it would be best if the DB stored
# the tree data.  The tinderbox server could download the data from a
# central place when it first starts up, and this down load could
# trigger a registration for email updates of changes to any trees
# which this tinderbox server is monitoring. As long as the data is
# stored in a file, it is safer to have this data stored in VC in its
# own file, in case of problems.  The data tends to be technical
# enough that this file is managed by a Jr Sysadmin and they prefer a
# text editor with CVS to a CGI script and no records.  Also the
# client side build script needs to know how to checkout a tree and
# map tree names to what was checked out, so there is a communication
# issue to work out.


# $Revision: 1.20 $ 
# $Date: 2003/08/04 17:15:07 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/TreeData.pm,v $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




package TreeData;

# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.

$VERSION = '#tinder_version#';


# Each tree is a map from a name to all the information necessary to
# do a checkout of the sources (in this case cvs repository to
# checkout, module, branch needed) and to create a URL to display the
# page (if such a means exists).  Tree names are used as parts of
# filenames so should not contain ' ' or '/' or unprintable
# characters.  You may need to change this datastructure if you do not
# use CVS.

# Who uses tree data?

# Only the VCDisplay and TinderDB::VC know what the internal structure
# of the tree hash is.  All other modules only need to use the name of
# a valid tree.

# Also the client side build script needs to know how to checkout a
# tree.


#------------------------------------------------------------------


# This section describes some functionality issues when using
# VC_CVS.pm and is not applicable to Bonsai users.


# The 'cvs history' command does not give correct info about
# modifications to the repository (types= 'ARM') when passed a module
# name, it was only designed to work on individual files.  If you use
# the -m option you will not find any info about checkins. We leave
# off the module name and take the info for the whole repository
# instead. If this becomes a bother we can reject information about
# updates which do not match a pattern.  In this respect the pattern
# becomes a proxy for the module.  The pattern is stored in
# $TreeData::VC_TREE{$tree}{'dir_pattern'}.  

# There is nothing I can think of to get information about which
# branch the changes were checked in on.  The history command also has
# no notion of branches, but that is a common problem with the way CVS
# does branching anyway.

# What we really want it the checkin comments but history does not
# give us that information and it would be too expensive to run cvs
# again for each file.  The file name is good enough but other VC
# implementations should use the checkin comments if available.

# dir_pattern: Reject all CVS update file names which do not match
# this pattern. This is only used by VC_CVS.pm and not by Bonsai.
# This is a hack to get around the fact that CVS does not always
# understand the module structure, and 'cvs history' will report all
# updates to a repository even for modules not of interest to us.  So
# if you use VC_CVS.pm the module value will not be passed to CVS. If
# the user can describe what is in the module using a perl pattern
# then VC_CVS.pm can implement lost CVS module functionality in the
# perl code.

# module: Although VC_CVS ignores this variable you should still set
# it because it is clearer (a tree is supposed to map to a module and
# branch) and for practical reasons (the VC_Display may use it an CVS
# may allow proper use in the future).


#------------------------------------------------------------------

# Bonsai specific information.

# Bonsai designates one tree to be the default tree and stores its
# state (open/closed) in a different directory.  We note this tree by
# adding an elment to the hash 'is_bonsai_default' which is set for
# this tree.

# If you are using bonsai you MUST use the local format for specifying
# the CVS tree.  Bonsai expects the root to be a directory which
# contains the CVS repository.

#------------------------------------------------------------------


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
# or
#
#                          root => '/devel/java_repository',
#
#			  module => 'SeaMonkeyAll',
#			  branch => 'main',
#                         dir_pattern => '^/mozilla',
#                         is_bonsai_default => 1,
#			 },

#	    # these are dummy trees for testing.	    
#
	    'Project_A' =>  {
                   root => '/cvsroot',
                   module => '',
                   branch => 'HEAD',
                  },
	    'Project_B' =>  {
                   root => '/cvsroot',
                   module => 'MozillaTinderboxAll',
                   branch => 'HEAD',
                  },
	    'Project_C' =>  {
                   root => '/cvsroot',
                   module => 'NSS',
                   branch => 'HEAD',
                  },


# ------------- Real Trees Go Here ---------- 
#

#	    'SeaMonkey' =>  {
#                   root => '/cvsroot',
#                   module => 'MozillaTinderboxAll',
#                   branch => 'HEAD',

                   # If you are using Perforce use perforce filespec
                   # to specify branche/module pairs. Perforce blurs
                   # the distincution between a branch and a module.

                   # Tinderbox notion of tree is a good match for the
                   # perforce notion of filespec.  The tinderbox
                   # notion of module and branch are a bit contrived
                   # in a perforce setting.

                   # I assume that the branch is the prefix of the
                   # filespex and the module is the suffix of the
                   # filespec.  The funtion Tree2Filespec is more
                   # important then the way you store the tree data.

                   # In these Perforce examples I assume that the
                   # filespec will be $branch.$module

                   # Here is an example of "everything"

                   # branch => '//',
                   # module => '...',

                   # Here is an example of a branch "2.5" which is
                   # stored in a directory called releases.  There is
                   # a subdirectory which contains the "web" part of
                   # this release.

                   # branch => '//releases/v2.5',
                   # module => '/webmodule/...',
#                  },


	   );

# what to append to a user name from the VC system to turn it into a
# mail address.  The Mozilla/Netscape people do not need this because
# they incode the Mail address in the user names.

$VCMAIL_SUFFIX = '@mozilla.org';


sub TreeName2Root {
    my ($treename) = @_;

    my $root = $VC_TREE{$treename}{'root'};

    return $root;
}

sub TreeName2Module {
    my ($treename) = @_;

    my $module = $VC_TREE{$treename}{'module'};

    return $module;
}

sub TreeName2Branch {
    my ($treename) = @_;

    my $branch = $VC_TREE{$treename}{'branch'};

    return $branch;
}

sub TreeName2DirPattern {
    my ($treename) = @_;

    my $dir_pattern = $VC_TREE{$treename}{'dir_pattern'};

    return $dir_pattern;
}

# This is a Peforce specific function.  Here is where we convert the
# Tinderbox notion of tree into the perforce notion of a filespec.
# These two ideas are good matches for eachother but the notions of
# branch and module are a bit contrived.  I allow the users to define
# exactly how the mapping from branch/module to filespec takes place
# though this function is probably suitable for most uses.

sub Tree2Filespec {
    my ($treename) = @_;

    my $branch = $VC_TREE{$treename}{'branch'};
    my $module = $VC_TREE{$treename}{'module'};

    my $filespec = $branch.$module;

    # This quick test for a reasonable filespec, and fix for strings
    # which look wrong, comes in handy when I am testing (I tend to
    # focus on test data which looks like CVS branch/modules) and may
    # help a user who has totally botched their branch/module
    # specifications to at least get a working system.

    ($filespec =~ m!//.*\.\.\.!) ||
        ($filespec = '//...');

   return $filespec;
}

sub TreeName2is_bonsai_default {
}


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

%TREE_STATES = (
		     'Open' => {
                         "html_color" => "white",
                         "hdml_char" => "O",
                         "order" => 1,
                         "description" => "All allowed",
                     },

		     'Closed' => {
                         "html_color" => "silver",
                         "hdml_char" => "C",
                         "order" => 2,
                         "description" => "Require approval of QA",
                     },

		     'Restricted' => {
                         "html_color" => "#e7e7e7", # a light grey
                         "hdml_char" => "R",
                         "order" => 3,
                         "description" => "Require approval of Drivers",
                     },

		     'Metered' => {
                         "html_color" => '#eeff00', # a yellow
                         "hdml_char" => "M",
                         "order" => 4,
                         "description" => "Require Current Sheriff approval",
                     },

		    );


# Sort the buildnames, into a display order.  It may make sense to
# define a local replacement function which sorts different trees into
# locally meaningful orders (build columns in dependency order, test
# columns last, etc)

sub sort_tree_buildnames {
  my ($tree, $buildnames_ref) = @_;

  my (@out) = sort @{ $buildnames_ref };

  return @out;
}



sub TreeState2color {
  my ($state) = @_;

  $color = $TREE_STATES{$state}{'html_color'};

  return $color;
}


sub TreeState2char {
  my ($state) = @_;

  $char = $TREE_STATES{$state}{'hdml_char'};

  return $char;
}

# convert a list of status strings into a list of hdml_chars

sub TreeStates2descriptions {
  my (@states) = @_;
  my (@out);

  for ($i=0; $i <= $#states; $i++) {
    my ($state) = $states[$i];
    my ($out) = $TREE_STATES{$state}{'description'};
    push @out, $out;
  }

  return @out;
}

sub get_all_sorted_tree_states {
  my @tree_states;

  @tree_states = (
                  map { $_->[0] }
                  sort{ $a->[1] <=> $b->[1] }	
                  map { [ $_, $TREE_STATES{$_}{'order'} ] }
                  (keys %TREE_STATES)
                  );
  
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


# convert a name as given by the version control system into a email
# address.


sub VCName2MailAddress {
    my ($vc_name) = (@_);

    $mail_addr = $vc_name;

    # The Netscape/Mozilla people have the CVS name be the same as the
    # mail address with a single character subsitution.

    if ($mail_addr =~ m/%/) {
        $mail_addr =~ s/%/@/;
    } else {
        $mail_addr .= $VCMAIL_SUFFIX;
    }

    return $mail_addr;
}

1;
