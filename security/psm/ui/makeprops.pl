#!/usr/bin/perl
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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
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

#
# collapse - given an input file, collapses multi-line strings into
#            single-line crunched strings.
#

$DEBUG = 0;

sub MakeNLSSafe
{
    $text = shift;

    # Change all single quotes (') to quoted single quotes ('').
    $text =~ s/\'/\'\'/g;
    # Now, quote every left brace not followed by a digit.
    #$text =~ s/\{([^0-9])/\'{\'$1/g;
    
    return $text;
}

sub Report
{
    ($prefix, $file, $line, $text) = @_;
    print STDERR "$file:$line: $prefix $text\n";
}

sub Warn 
{
    ($file, $line, $text) = @_;
    &Report("Warning - ", $file, $line, $text);
}

sub Error
{
    ($file, $line, $text) = @_;
    &Report("Error - ", $file, $line, $text);
    die;
}

sub ConvertPropsFile
{
    ($inFile, $outFile) = @_;
    #print "infile = $inFile\n";
    #print "outfile = $outFile\n";

    open(INFILE, $inFile) || die "Cannot open input file: $!";
    open(OUTFILE, ">$outFile") || die "Cannot open output file: $!";

    $block = 0; # are we in the middle of condensing a multi-line string?
    $blockText = ''; # accumulating line of text to be written out
    $currKey = ''; # key of currently accumulating string 
                      # (use to check for end)
    $accumDest = ''; # Property (if any) into which we are accumulating text
    #%accumProps = {}; # Properties being accumulated, if any

    $comment = 0; # are we inside a comment block?
    $line = 0;

    while(<INFILE>)
    {
        $line++;
        chop;
        if ($block) # block accumulation takes precedence over comments
        {
            # accumulating a block. 
            
            # look for ":<identifier>" where $currKey is the 
            # original identifier
            if ((m/^\s*:([^\s]+)\s*$/) && ($1 eq $currKey))
            {
                # end of block. output what we've accumulated.
                $outText = &MakeNLSSafe($blockText);
                print OUTFILE $currKey, "=", $outText, $/;
                $blockText = $currKey = '';
                $block = 0;
            }
            else
            {
                # just another line to add to the string.
                $blockText .= $_ . '<psm:cr><psm:lf>';
            }
        }
        elsif ((m/^;/) || (m/^\s*$/)) # matching '; comment' or all whitespace
        {
            # nothing, skip this line
        }
        elsif (m/^\#(.*)/) # matching '#ifdef', '#include', '#endif', etc.
        {
            # Look for directives we're interested in.
            $directive = $1;
            if ($directive =~ m/^pragma\s+([^\s]+)\s+(.*)$/)
            {
                # Got a "pragma". Find out what kind it is.
                $symbol = $1;
                $params = $2;
                if ($symbol eq 'begin_wrap_glossary')
                {
                    &Warn($inFile, $line, "Spaces in filename: $params") 
                        if ($params =~ m/\s+/);
                    $accumDest = $params;
                    #print STDERR "#pragma begin_wrap_glossary $accumDest\n";
                }
                elsif ($symbol eq 'end_wrap')
                {
                    $accumDest = $params;
                    #print STDERR "#pragma end_wrap $accumDest\n";
                }
                else
                {
                    &Warn($inFile, $line, "Unrecognized #pragma: $symbol");
                }
            }
            elsif ($directive =~ m/^ifdef\s+(.*)$/)
            {
                # Got an "ifdef". Look for definitions.
                $symbol = $1;
                #print STDERR "#ifdef $symbol\n";
            }
        }
        elsif (m/^([^=]+):\s*$/) # matching '<identifier>:'
        {
            # found a block. start accumulating. (drop this line)
            $currKey = $1;
            $block = 1;
            $blockText='';
        }
        elsif (m/^([^=]*)=\"(.*)\"$/) # matching 'key="value"'
        {
            # line with quotes. remove the enclosing quotes.
            $outText = &MakeNLSSafe($2);
            
            print OUTFILE $1, "=", $outText, $/;
        }
        else 
        {
            # generic line not matching anything special, just output and move on
            print OUTFILE $_, $/;
        }
    }

    if ($block)
    {
        # Leftover block of text. Spit it out.
        &Warn($inFile, "<end>", "Couldn't find end of `$currKey'.");
        &Warn($inFile, "<end>", 
             "Anything after `$currKey' will not be accessible.");
        $outText = &MakeNLSSafe($blockText);
        print OUTFILE $currKey, "=", $outText, $/;
    }

    close(INFILE);
    close(OUTFILE);
}

# Process each file.
$inFile = shift(@ARGV);
$outFile = shift(@ARGV);

&ConvertPropsFile($inFile, $outFile);
