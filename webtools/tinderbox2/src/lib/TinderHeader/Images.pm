# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# NOT WORKING - please port, following other Headers as a guide.

# TinderHeader::Images - A random image and caption which is displayed
# on the status page.  It is similar to the old program 'fortune' and
# is one of the few TinderHeaders not set by the administrators.
# Images are added by the users.


# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).


package TinderHeader::Images;

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

# load the simple name of this module into TinderHeader so we can
# track the implementations provided.

$TinderHeader::NAMES2OBJS{ 'Images' } = 
  TinderHeader::Images->new();



sub gettree_header {
    local(@log,@ret,$i);
    
    (-r $filename) || 
      return '';
    
    require($filename) ||
      die("Could not eval filename: '$filename': $!\n");
    


# See http://www.ugcs.caltech.edu/~werdna/gifsize/ for more information

# gifsize is a perl routine which will read in an HTML file and insert
# HEIGHT=### WIDTH=### directives into the inlined images used in the
# file.
#
# you can do something like: 
#
#	find . -name \*.html -exec gifsize {} \;
#	find . -name \*.html.bak -exec rm -f {} \;. 

            <p><center><a href=addimage.cgi><img src="$rel_path$imageurl"
              width=$imagewidth height=$imageheight><br>
              $quote</a><br>
            </center>
            <p>



$pick_random_line = sub {
  # return a random line
  srand;
  @ret = @{ $value[rand scalar(@log)] };
}

       $value = "@{[&$pick_random_line()]}"
   return @ret;
}


sub savetree_header {

  my (@log, @ret, $lineno, );

  my $file = "$data_dir/imagelog.txt";

  (-r $file) || return ;
  open(IMAGELOG, "<$file" )
    or die("can't open file: $file. $!\n");

  @log = <IMAGELOG>;

  close(IMAGELOG)
    or die("can't open file: $file. $!\n");

  # return a random line
  srand;
  my $lineno = rand @log;
  @ret = split(/\`/,$log[$linno]);

  return @ret;
}

1;
