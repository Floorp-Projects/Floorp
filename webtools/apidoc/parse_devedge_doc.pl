#!/usr/bin/perl
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is JavaScript Core Tests.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1997-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#

#  Parses the nasty devedge document (jsref.htm) into somewhat nicer xml.
#  The XML source, jsref.xml, located at
#  http://www.mozilla.org/js/apidoc/jsapi.xml is now the authoritative place to
#  make changes.
#  This file should be considered dead.  Do not use it.  It is here for emergency
#  use only.
#  REPEAT: DO NOT USE THIS SCRIPT!

my $API         = 0;
my $ENTRY       = 1;
my $TYPE        = 2;
my $SUMMARY     = 3;
my $SYNTAX      = 4;
my $PARAM       = 5;
my $RETVAL      = 6;
my $DESCRIPTION = 7;
my $EXAMPLE     = 8;
my $NOTE        = 9;
my $SEE_ALSO    = 10;
my $DEPRECATED  = 11;
my $EXTERNALREF = 12;
my $GROUP       = 13;
my $C           = 14;
my $P           = 15;
my $B           = 16;
my $I           = 17;
my $COMPLETED   = 18;
my @TAGS = ("API", "ENTRY", "TYPE", "SUMMARY", "SYNTAX", "PARAM", "RETVAL",
            "DESCRIPTION", "EXAMPLE", "NOTE", "SEEALSO", "DEPRECATED", 
            "EXTERNALREF", "GROUP", "C", "P", "B", "I");

my $NAMESPACE = ""; # "APIDOC";

my $TAB = "  ";
my $indent_pfx = "";

die ("I said, don't use this script!  Don't you read the comments?\n");

&parse_old_doc ();

sub parse_old_doc {
    my $done = 0;

    &output ("<?xml version='1.0' encoding='ISO-8859-1' standalone='yes'?>");
    &output ("<!DOCTYPE api>");
    &open_container ($TAGS[$API], "id", "JavaScript-1.5");
    &tag ($TAGS[$EXTERNALREF], "name", "LXR ID Search", "value",
          "http://lxr.mozilla.org/seamonkey/ident?i={e}");
    my $line = <>;

    while (!$done) {
        if ($line =~ /\<H1\>\<A NAME\=\"(.*)\"\>\<\/A\>/i) {
            # beginning of a new API entry
            $line = &parse_entry ($1);
        } else {
            $line = "";
        }
        if ($line eq "") {
            $line = <> or $done = 1;
        }
    }
    &close_container ($TAGS[$API]);
}
            
sub parse_entry {
    my $deprecated = 0;
    my ($name) = @_;
    my $line, $partialline = "";
    my $mode = $ENTRY, $submode = 0;

    &open_container ($TAGS[$mode], "id", $name);
    
    while ($mode != $COMPLETED) {
        if ($partialline) {
            $line = $partialline;
            $partialline = "";
        } else {
            $line = <>;
        }

        if ($line =~ /\<H1\>\<A NAME\=\"(.*)\"\>\<\/A\>/i) {
            # oops, somehow we're at the next entry
            # stop what we're doing and
            # return this line to the caller
            &close_container ($TAGS[$mode]);
            $partialline = $line;
            $mode = $COMPLETED;
        } elsif ($mode == $ENTRY) {
            if ($line =~ /\<\/A\>\<\/H1\>/i) {
                $mode = $TYPE;
            }
        } elsif ($mode == $TYPE) {
            if ($line =~ /\<A NAME=\"\d+\"\>/i) {
                # useless line
            } elsif ($line =~ /([^\.]+).\s+(.*)/) {
                &tag ($TAGS[$mode], "value", $1);
                if ($2) {
                    $mode = $SUMMARY;
                    &open_container ($TAGS[$mode]);
                    $partialline = $2;
                }
                if ($line =~ /deprecated/i) {
                    $deprecated = 1;
                }
            }
        } elsif ($mode == $SUMMARY) {
            if ($line =~ /deprecated/i) {
                $deprecated = 1;
            }
            if ($line =~ /(.*)?(\<\/A\>\<\/P\>)$/) {
                &output (&fixup($1));
                # if line ended with </A></P>, we're at the end of the summary
                &close_container ($TAGS[$mode]);
                $mode = $SYNTAX;
            } else {
                &output ($line);
            }
        } elsif (($mode == $SYNTAX) || ($mode == $DESCRIPTION) ||
                 ($mode == $EXAMPLE) || ($mode == $NOTE)) {
            if ($submode == 0) {
                if ($line =~ /\<\/A\>\<\/H4\>/) {
                    if ($mode == $EXAMPLE) {
                        $line = <>;
                        $line = <>; # eat two useless lines
                        $line = <>; # next is the description
                        &open_container ($TAGS[$mode], "desc",
                                         &fixup_all($line));
                        # are you having fun yet?
                    } else {
                        &open_container ($TAGS[$mode]);
                    }
                    $submode = 1;
                }
            } elsif ($submode == 1) {
                if ($line =~ /<H4><A NAME="Head3;"><\/A>/) {
                    #start of a new section
                    &close_container ($TAGS[$mode]);
                    $submode = 0;
                    $line = <>;
                    $mode = $COMPLETED;
                    if ($line =~ /\<A NAME\=\"\d+\">/) {
                        $line = <>;
                        if ($line =~ /(description|example|note|see also)/i) {
                            if (lc($1) eq "description") {
                                $mode = $DESCRIPTION;
                            } elsif (lc($1) eq "example") {
                                $mode = $EXAMPLE;
                            } elsif (lc($1) eq "note") {
                                $mode = $NOTE;
                            } else {
                                $mode = $SEE_ALSO;
                            }
                        }
                    }
                } elsif ($line =~ /<BLOCKQUOTE><B>NOTE:/) {
                    # note embedded in description section
                    &close_container ($TAGS[$mode]);
                    $mode = $NOTE;
                    &open_container ($TAGS[$mode]);
                } elsif (($mode == $NOTE) && ($line =~ /<\/BLOCKQUOTE>/)) {
                    # note over
                    output ("<P/>\n");
                    &close_container ($TAGS[$mode]);
                    $mode = $DESCRIPTION;
                    &open_container ($TAGS[$mode]);
                } elsif (($mode == $SYNTAX) || ($mode == $EXAMPLE)) {
                    if (($mode == $SYNTAX) && 
                        ($line =~ /(.*)<TABLE BORDER="0">/)) {
                        # syntax parameter table start
                        $line = $1;
                        $submode = 2;
                    }
                    $_ = $line;
                    s/<br>/\n/ig;
                    s/<[^>]*>//g;
                    s/^\s*//g;
                    s/\s*$//g;
                    s/&nbsp;/ /g;
                    if (/[\S\N]/) {
                        print ("$_\n");
                    }
                } elsif ($line =~ /(.*)<TABLE BORDER="0">/) {
                    &output (&fixup($1));
                    &output ("<![CDATA[");
                    &output ('<TABLE BORDER="0">');
                    $submode = 2;
                } elsif ($line =~ /<ul><P><LI>/) {
                    &output ("<![CDATA[");
                    &output (&fixup_cdata($line));
                    $submode = 2;
                } else {
                    # if all else f[l]ails, just print it.
                    &output (&fixup($line));
                }
            } elsif (($mode == $SYNTAX) && ($submode == 2)) {
                # inside parameter list table
                my $col = 0;
                my $name, $type, $desc = "";
                $line = <>;
                while (!($line =~ /<\/TABLE>/)) {
                    if ((($col == 0) || ($col == 1)) &&
                        ($line =~ /(.*)<\/A><\/P>/)) {
                        $1 =~ /<CODE>(.*)<\/CODE>/;
                        if ($col == 0) {
                            $name = $1;
                            $col = 1;
                        } else {
                            $type = $1;
                            $col = 2;
                        }
                    } elsif (($col == 2) &&
                             (!($line =~
                                /<TR><TD VALIGN=baseline ALIGN=left>/))) {
                        $desc .= &fixup_more ($line);
                    } elsif ($col == 2) {
                        &open_container ($TAGS[$PARAM], "name", $name,
                                         "type", $type);
                        &output ($desc);
                        &close_container ($TAGS[$PARAM]);
                        $name = "";
                        $type = "";
                        $desc = "";
                        $col = 0;
                    }
                    $line = <>;
                }
                &open_container ($TAGS[$PARAM], "name", $name,
                                 "type", $type);
                &output ($desc);
                &close_container ($TAGS[$PARAM]);
                $submode = 1;
            } elsif ($submode == 2) {
                if (($line =~ /<\/TABLE>/) || ($line =~ /<\/ul>/)) {
                    &output (&fixup_cdata ($line));
                    &output ("]]>");
                    $submode = 1;
                } else {
                    &output (&fixup_cdata ($line));
                }
            }   
        } elsif ($mode == $SEE_ALSO) {
            if ($line =~ /(.*)<\/P>/) {
                my $list = &fixup ($1);
                my @refs = split(/\s*,\s*/, $list);
                for (@refs) {
                    if ($_) {
                        $_ =~ /^\s*(\S+)\s*$/;
                        &tag ($TAGS[$mode], "value", $1);
                    }
                }
                $mode = $COMPLETED;
            }
        } else {
            die ("Unknown mode '$mode' encountered.\n");
        }
    }

    if ($mode != $COMPLETED) {
        die ("Bad state");
    }

    if ($deprecated) {
        &tag ($TAGS[$DEPRECATED]);
    }
    &close_container ($TAGS[$ENTRY]);
    
    return $partialline;
        
}

sub output {
    my ($line) = @_;
    s/^\s+//;
    s/\s+$//;
    if (!($line =~ /^\n*$/)) {
        $_ = $line;
        s/\n*$//;
        s/^\s*\n*//;
        if (/[\S\N]/) {
            print $indent_pfx . $_ . "\n";
        }
    }
}

sub fixup_all {
# wipe out ALL markup
    ($_) = @_;
    s/<[^>]*>//g;
    s/\n//g;
    return $_;
}    

sub fixup_more {
# all fixups, plus paragraph->br
    ($_) = @_;
    $_ = &fixup($_);
    s/<P\/>/<BR\/>/g;
    return $_;
}

sub fixup {
# clean up all markup, and change HTML tags to XML tags
    ($_) = @_;

    s/<CODE>/<C>/g;
    s/<\/CODE>/<\/C>/g;
    s/<BR>//ig;    
    s/<\/P>|<P>/<P\/>/g;
    s/(<P\/>)+/<P\/>/g;
    return &fixup_cdata($_);
}

sub fixup_cdata {
# clean up markup for use in <![CDATA[ tags
    ($_) = @_;
    s/<TD VALIGN=(.*) ALIGN=([^>]*)>/<TD VALIGN='$1' ALIGN='$2'\>/g;
    s/<TR><TD VALIGN='baseline' ALIGN='left'><P>/<\/TD><\/TR><TR> <TD VALIGN='baseline' ALIGN='left'><P>/g;
    s/<\/B><\/A><TD VALIGN='baseline' ALIGN='left'>/<\/B><\/TD><TD VALIGN='baseline' ALIGN='left'>/ig;
    s/<\/P><TD VALIGN='baseline' ALIGN='left'>/<\/TD><TD VALIGN='baseline' ALIGN='left'>/g;
    s/<\/TABLE>/<\/TD><\/TR><\/TABLE >/g;
    s/<PRE>|<\/PRE>//g;
    s/<BR>/<BR\/>/ig;
    s/\<A[^\>]+\>//g;
    s/\<\/A\>//g;
    return $_;
}

sub open_container {
    my ($tag_name) = @_;
    &open_tag (1, @_);
    $indent_pfx .= $TAB;
}

sub tag {
    &open_tag (0, @_);
}

sub open_tag {
    my $iscontainer = shift(@_);
    my $tagname = shift(@_);
    my @attrs = @_;
    my $attrname;
    my $tag = "";

    $tag .= "<";

    if ($NAMESPACE) {
        $tag .= "$NAMESPACE:";
    }
    
    $tag .= $tagname;

    while ($attrname = shift(@attrs)) {
        $tag .= " $attrname='" . shift (@attrs) . "'";
    }

    if (!$iscontainer) {
        $tag .= "/";
    }

    $tag .= ">";
    &output ($tag);
}

sub close_container {
    my ($tag_name) = @_;

    substr($indent_pfx, 0, length($TAB)) = "";
    my $tag = "</";
    if ($NAMESPACE) {
        $tag .= "$NAMESPACE:";
    }
    &output ($tag . "$tag_name>");
}
