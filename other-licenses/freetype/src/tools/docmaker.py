#!/usr/bin/env python
#
#  DocMaker 0.1 (c) 2000-2001 David Turner <david@freetype.org>
#
#  DocMaker is a very simple program used to generate the API Reference
#  of programs by extracting comments from source files, and generating
#  the equivalent HTML documentation.
#
#  DocMaker is very similar to other tools like Doxygen, with the
#  following differences:
#
#    - It is written in Python (so it is slow, but easy to maintain and
#      improve).
#
#    - The comment syntax used by DocMaker is simpler and makes for
#      clearer comments.
#
#  Of course, it doesn't have all the goodies of most similar tools,
#  (e.g. C++ class hierarchies), but hey, it is only 2000 lines of
#  Python.
#
#  DocMaker is mainly used to generate the API references of several
#  FreeType packages.
#
#   - David
#

import fileinput, sys, os, time, string, glob, getopt

# The Project's title.  This can be overridden from the command line with
# the options "-t" or "--title".
#
project_title = "Project"

# The project's filename prefix.  This can be set from the command line with
# the options "-p" or "--prefix"
#
project_prefix = ""

# The project's documentation output directory.  This can be set from the
# command line with the options "-o" or "--output".
#
output_dir = None


# The following defines the HTML header used by all generated pages.
#
html_header_1 = """\
<html>
<header>
<title>"""

html_header_2= """ API Reference</title>
<basefont face="Verdana,Geneva,Arial,Helvetica">
<style content="text/css">
  P { text-align=justify }
  H1 { text-align=center }
  LI { text-align=justify }
</style>
</header>
<body text=#000000
      bgcolor=#FFFFFF
      link=#0000EF
      vlink=#51188E
      alink=#FF0000>
<center><h1>"""

html_header_3=""" API Reference</h1></center>
"""

# This is recomputed later when the project title changes.
#
html_header = html_header_1 + project_title + html_header_2 + project_title + html_header_3


# The HTML footer used by all generated pages.
#
html_footer = """\
</body>
</html>"""

# The header and footer used for each section.
#
section_title_header = "<center><h1>"
section_title_footer = "</h1></center>"

# The header and footer used for code segments.
#
code_header = "<font color=blue><pre>"
code_footer = "</pre></font>"

# Paragraph header and footer.
#
para_header = "<p>"
para_footer = "</p>"

# Block header and footer.
#
block_header = "<center><table width=75%><tr><td>"
block_footer = "</td></tr></table><hr width=75%></center>"

# Description header/footer.
#
description_header = "<center><table width=87%><tr><td>"
description_footer = "</td></tr></table></center><br>"

# Marker header/inter/footer combination.
#
marker_header = "<center><table width=87% cellpadding=5><tr bgcolor=#EEEEFF><td><em><b>"
marker_inter  = "</b></em></td></tr><tr><td>"
marker_footer = "</td></tr></table></center>"

# Source code extracts header/footer.
#
source_header = "<center><table width=87%><tr bgcolor=#D6E8FF width=100%><td><pre>"
source_footer = "</pre></table></center><br>"

# Chapter header/inter/footer.
#
chapter_header = "<center><table width=75%><tr><td><h2>"
chapter_inter  = "</h2><ul>"
chapter_footer = "</ul></td></tr></table></center>"

current_section = None


# This function is used to sort the index.  It is a simple lexicographical
# sort, except that it places capital letters before lowercase ones.
#
def index_sort( s1, s2 ):
    if not s1:
        return -1

    if not s2:
        return 1

    l1 = len( s1 )
    l2 = len( s2 )
    m1 = string.lower( s1 )
    m2 = string.lower( s2 )

    for i in range( l1 ):
        if i >= l2 or m1[i] > m2[i]:
            return 1

        if m1[i] < m2[i]:
            return -1

        if s1[i] < s2[i]:
            return -1

        if s1[i] > s2[i]:
            return 1

    if l2 > l1:
        return -1

    return 0


# Sort input_list, placing the elements of order_list in front.
#
def sort_order_list( input_list, order_list ):
    new_list = order_list[:]
    for id in input_list:
        if not id in order_list:
            new_list.append( id )
    return new_list


# Translate a single line of source to HTML.  This will convert
# a "<" into "&lt.", ">" into "&gt.", etc.
#
def html_quote( line ):
    result = string.replace( line,   "&", "&amp;" )
    result = string.replace( result, "<", "&lt;" )
    result = string.replace( result, ">", "&gt;" )
    return result

# same as 'html_quote', but ignores left and right brackets
#
def html_quote0( line ):
    return string.replace( line, "&", "&amp;" )


# Open the standard output to a given project documentation file.  Use
# "output_dir" to determine the filename location if necessary and save the
# old stdout in a tuple that is returned by this function.
#
def open_output( filename ):
    global output_dir

    if output_dir and output_dir != "":
        filename = output_dir + os.sep + filename

    old_stdout = sys.stdout
    new_file   = open( filename, "w" )
    sys.stdout = new_file

    return ( new_file, old_stdout )


# Close the output that was returned by "close_output".
#
def close_output( output ):
    output[0].close()
    sys.stdout = output[1]


# Check output directory.
#
def check_output( ):
    global output_dir
    if output_dir:
        if output_dir != "":
            if not os.path.isdir( output_dir ):
                sys.stderr.write( "argument" + " '" + output_dir + "' " +
                                  "is not a valid directory" )
                sys.exit( 2 )
        else:
            output_dir = None


def compute_time_html( ):
    global html_footer
    time_string = time.asctime( time.localtime( time.time() ) )
    html_footer = "<p><center><font size=""-2"">generated on " + time_string + "</font></p></center>" + html_footer

# The FreeType 2 reference is extracted from the source files.  These
# contain various comment blocks that follow one of the following formats:
#
#  /**************************
#   *
#   *  FORMAT1
#   *
#   *
#   *
#   *
#   *************************/
#
#  /**************************/
#  /*                        */
#  /*  FORMAT2               */
#  /*                        */
#  /*                        */
#  /*                        */
#  /*                        */
#
#  /**************************/
#  /*                        */
#  /*  FORMAT3               */
#  /*                        */
#  /*                        */
#  /*                        */
#  /*                        */
#  /**************************/
#
# Each block contains a list of markers; each one can be followed by
# some arbitrary text or a list of fields.  Here an example:
#
#    <Struct>
#       MyStruct
#
#    <Description>
#       this structure holds some data
#
#    <Fields>
#       x :: horizontal coordinate
#       y :: vertical coordinate
#
#
# This example defines three markers: 'Struct', 'Description' & 'Fields'.
# The first two markers contain arbitrary text, while the last one contains
# a list of fields.
#
# Each field is simply of the format:  WORD :: TEXT...
#
# Note that typically each comment block is followed by some source code
# declaration that may need to be kept in the reference.
#
# Note that markers can alternatively be written as "@MARKER:" instead of
# "<MARKER>".  All marker identifiers are converted to lower case during
# parsing in order to simply sorting.
#
# We associate with each block the following source lines that do not begin
# with a comment.  For example, the following:
#
#   /**********************************
#    *
#    * <mytag>  blabla
#    *
#    */
#
#   bla_bla_bla
#   bilip_bilip
#
#   /* - this comment acts as a separator - */
#
#   blo_blo_blo
#
#
# will only keep the first two lines of sources with
# the "blabla" block.
#
# However, the comment will be kept, with following source lines if it
# contains a starting '#' or '@' as in:
#
#   /*@.....*/
#   /*#.....*/
#   /* @.....*/
#   /* #.....*/
#



#############################################################################
#
# The DocCode class is used to store source code lines.
#
#   'self.lines' contains a set of source code lines that will be dumped as
#   HTML in a <PRE> tag.
#
#   The object is filled line by line by the parser; it strips the leading
#   "margin" space from each input line before storing it in 'self.lines'.
#
class DocCode:

    def __init__( self, margin = 0 ):
        self.lines  = []
        self.margin = margin


    def add( self, line ):
        # remove margin whitespace
        #
        if string.strip( line[: self.margin] ) == "":
            line = line[self.margin :]
        self.lines.append( line )


    def dump( self ):
        for line in self.lines:
            print "--" + line
        print ""


    def get_identifier( self ):
        # this function should never be called
        #
        return "UNKNOWN_CODE_IDENTIFIER!"


    def dump_html( self, identifiers = None ):
        # clean the last empty lines
        #
        l = len( self.lines ) - 1
        while l > 0 and string.strip( self.lines[l - 1] ) == "":
            l = l - 1

        # The code footer should be directly appended to the last code
        # line to avoid an additional blank line.
        #
        print code_header,
        for line in self.lines[0 : l+1]:
            print '\n' + html_quote(line),
        print code_footer,



#############################################################################
#
# The DocParagraph is used to store text paragraphs.
# 'self.words' is simply a list of words for the paragraph.
#
# The paragraph is filled line by line by the parser.
#
class DocParagraph:

    def __init__( self ):
        self.words = []


    def add( self, line ):
        # Get rid of unwanted spaces in the paragraph.
        #
        # The following two lines are the same as
        #
        #   self.words.extend( string.split( line ) )
        #
        # but older Python versions don't have the `extend' attribute.
        #
        last = len( self.words )
        self.words[last : last] = string.split( line )


    # This function is used to retrieve the first word of a given
    # paragraph.
    #
    def get_identifier( self ):
        if self.words:
            return self.words[0]

        # should never happen
        #
        return "UNKNOWN_PARA_IDENTIFIER!"


    def get_words( self ):
        return self.words[:]


    def dump( self, identifiers = None ):
        max_width = 50
        cursor    = 0
        line      = ""
        extra     = None
        alphanum  = string.lowercase + string.uppercase + string.digits + '_'

        for word in self.words:
            # process cross references if needed
            #
            if identifiers and word and word[0] == '@':
                word = word[1 :]

                # we need to find non-alphanumeric characters
                #
                l = len( word )
                i = 0
                while i < l and word[i] in alphanum:
                    i = i + 1

                if i < l:
                    extra = word[i :]
                    word  = word[0 : i]

                block = identifiers.get( word )
                if block:
                    word = '<a href="' + block.html_address() + '">' + word + '</a>'
                else:
                    word = '?' + word

            if cursor + len( word ) + 1 > max_width:
                print html_quote0(line)
                cursor = 0
                line   = ""

            line = line + word
            if not extra:
                line = line + " "

            cursor = cursor + len( word ) + 1


            # Handle trailing periods, commas, etc. at the end of cross
            # references.
            #
            if extra:
                if cursor + len( extra ) + 1 > max_width:
                    print html_quote0(line)
                    cursor = 0
                    line   = ""

                line   = line + extra + " "
                cursor = cursor + len( extra ) + 1
                extra  = None

        if cursor > 0:
            print html_quote0(line)

        # print "§" # for debugging only


    def dump_string( self ):
        s     = ""
        space = ""
        for word in self.words:
            s     = s + space + word
            space = " "

        return s


    def dump_html( self, identifiers = None ):
        print para_header
        self.dump( identifiers )
        print para_footer



#############################################################################
#
# DocContent is used to store the content of a given marker.
#
# The "self.items" list contains (field,elements) records, where "field"
# corresponds to a given structure field or function parameter (indicated
# by a "::"), or NULL for a normal section of text/code.
#
# Hence, the following example:
#
#   <MyMarker>
#      This is an example of what can be put in a content section,
#
#      A second line of example text.
#
#      x :: A simple test field, with some contents.
#      y :: Even before, this field has some code contents.
#           {
#             y = x+2;
#           }
#
# should be stored as
#
#     [ ( None, [ DocParagraph, DocParagraph] ),
#       ( "x",  [ DocParagraph ] ),
#       ( "y",  [ DocParagraph, DocCode ] ) ]
#
# in 'self.items'.
#
# The DocContent object is entirely built at creation time; you must pass a
# list of input text lines in the "lines_list" parameter.
#
class DocContent:

    def __init__( self, lines_list ):
        self.items  = []
        code_mode   = 0
        code_margin = 0
        text        = []
        paragraph   = None   # represents the current DocParagraph
        code        = None   # represents the current DocCode

        elements    = []     # the list of elements for the current field;
                             # contains DocParagraph or DocCode objects

        field       = None   # the current field

        for aline in lines_list:
            if code_mode == 0:
                line   = string.lstrip( aline )
                l      = len( line )
                margin = len( aline ) - l

                # if the line is empty, this is the end of the current
                # paragraph
                #
                if l == 0 or line == '{':
                    if paragraph:
                        elements.append( paragraph )
                        paragraph = None

                    if line == "":
                        continue

                    code_mode   = 1
                    code_margin = margin
                    code        = None
                    continue

                words = string.split( line )

                # test for a field delimiter on the start of the line, i.e.
                # the token `::'
                #
                if len( words ) >= 2 and words[1] == "::":
                    # start a new field - complete current paragraph if any
                    #
                    if paragraph:
                        elements.append( paragraph )
                        paragraph = None

                    # append previous "field" to self.items
                    #
                    self.items.append( ( field, elements ) )

                    # start new field and elements list
                    #
                    field    = words[0]
                    elements = []
                    words    = words[2 :]

                # append remaining words to current paragraph
                #
                if len( words ) > 0:
                    line = string.join( words )
                    if not paragraph:
                        paragraph = DocParagraph()
                    paragraph.add( line )

            else:
                # we are in code mode...
                #
                line = aline

                # the code block ends with a line that has a single '}' on
                # it that is located at the same column that the opening
                # accolade...
                #
                if line == " " * code_margin + '}':
                    if code:
                        elements.append( code )
                        code = None

                    code_mode   = 0
                    code_margin = 0

                # otherwise, add the line to the current paragraph
                #
                else:
                    if not code:
                        code = DocCode()
                    code.add( line )

        if paragraph:
            elements.append( paragraph )

        if code:
            elements.append( code )

        self.items.append( ( field, elements ) )


    def get_identifier( self ):
        if self.items:
            item = self.items[0]
            for element in item[1]:
                return element.get_identifier()

        # should never happen
        #
        return "UNKNOWN_CONTENT_IDENTIFIER!"


    def get_title( self ):
        if self.items:
            item = self.items[0]
            for element in item[1]:
                return element.dump_string()

        # should never happen
        #
        return "UNKNOWN_CONTENT_TITLE!"


    def dump( self ):
        for item in self.items:
            field = item[0]
            if field:
                print "<field " + field + ">"

            for element in item[1]:
                element.dump()

            if field:
                print "</field>"


    def dump_html( self, identifiers = None ):
        n        = len( self.items )
        in_table = 0

        for i in range( n ):
            item  = self.items[i]
            field = item[0]

            if not field:
                if in_table:
                    print "</td></tr></table>"
                    in_table = 0

                for element in item[1]:
                    element.dump_html( identifiers )

            else:
                if not in_table:
                    print "<table cellpadding=4><tr valign=top><td>"
                    in_table = 1
                else:
                    print "</td></tr><tr valign=top><td>"

                print "<b>" + field + "</b></td><td>"

                for element in item[1]:
                    element.dump_html( identifiers )

        if in_table:
            print "</td></tr></table>"


    def dump_html_in_table( self, identifiers = None ):
        n        = len( self.items )
        in_table = 0

        for i in range( n ):
            item  = self.items[i]
            field = item[0]

            if not field:
                if item[1]:
                    print "<tr><td colspan=2>"
                    for element in item[1]:
                        element.dump_html( identifiers )
                    print "</td></tr>"

            else:
                print "<tr><td><b>" + field + "</b></td><td>"

                for element in item[1]:
                    element.dump_html( identifiers )

                print "</td></tr>"



#############################################################################
#
# The DocBlock class is used to store a given comment block.  It contains
# a list of markers, as well as a list of contents for each marker.
#
#   "self.items" is a list of (marker, contents) elements, where 'marker' is
#   a lowercase marker string, and 'contents' is a DocContent object.
#
#   "self.source" is simply a list of text lines taken from the uncommented
#   source itself.
#
#   Finally, "self.name" is a simple identifier used to uniquely identify
#   the block. It is taken from the first word of the first paragraph of the
#   first marker of a given block, i.e:
#
#      <Type> Goo
#      <Description> Bla bla bla
#
#   will have a name of "Goo"
#
class DocBlock:

    def __init__( self, block_line_list = [], source_line_list = [] ):
        self.items    = []               # current ( marker, contents ) list
        self.section  = None             # section this block belongs to
        self.filename = "unknown"        # filename defining this block
        self.lineno   = 0                # line number in filename

        marker        = None             # current marker
        content       = []               # current content lines list
        alphanum      = string.letters + string.digits + "_"
        self.name     = None

        for line in block_line_list:
            line2  = string.lstrip( line )
            l      = len( line2 )
            margin = len( line ) - l

            if l > 3:
                ender = None
                if line2[0] == '<':
                    ender = '>'
                elif line2[0] == '@':
                    ender = ':'

                if ender:
                    i = 1
                    while i < l and line2[i] in alphanum:
                        i = i + 1
                    if i < l and line2[i] == ender:
                        if marker and content:
                            self.add( marker, content )
                        marker  = line2[1 : i]
                        content = []
                        line2   = string.lstrip( line2[i+1 :] )
                        l       = len( line2 )
                        line    = " " * margin + line2

            content.append( line )

        if marker and content:
            self.add( marker, content )

        self.source = []
        if self.items:
            self.source = source_line_list

        # now retrieve block name when possible
        #
        if self.items:
            first     = self.items[0]
            self.name = first[1].get_identifier()


    # This function adds a new element to 'self.items'.
    #
    #   'marker' is a marker string, or None.
    #   'lines'  is a list of text lines used to compute a list of
    #            DocContent objects.
    #
    def add( self, marker, lines ):
        # remove the first and last empty lines from the content list
        #
        l = len( lines )
        if l > 0:
            i = 0
            while l > 0 and string.strip( lines[l - 1] ) == "":
                l = l - 1
            while i < l and string.strip( lines[i] ) == "":
                i = i + 1
            lines = lines[i : l]
            l     = len( lines )

        # add a new marker only if its marker and its content list
        # are not empty
        #
        if l > 0 and marker:
            content = DocContent( lines )
            self.items.append( ( string.lower( marker ), content ) )


    def find_content( self, marker ):
        for item in self.items:
            if ( item[0] == marker ):
                return item[1]
        return None


    def html_address( self ):
        section = self.section
        if section and section.filename:
            return section.filename + '#' + self.name

        return ""  # this block is not in a section?


    def location( self ):
        return self.filename + ':' + str( self.lineno )


    def print_warning( self, message ):
        sys.stderr.write( "WARNING:" +
                          self.location() + ": " + message + '\n' )


    def print_error( self, message ):
        sys.stderr.write( "ERROR:" +
                          self.location() + ": " + message + '\n' )
        sys.exit()


    def dump( self ):
        for i in range( len( self.items ) ):
            print "[" + self.items[i][0] + "]"
            content = self.items[i][1]
            content.dump()


    def dump_html( self, identifiers = None ):
        types      = ['type', 'struct', 'functype', 'function',
                      'constant', 'enum', 'macro', 'structure', 'also']

        parameters = ['input', 'inout', 'output', 'return']

        if not self.items:
            return

        # start of a block
        #
        print block_header

        # place html anchor if needed
        #
        if self.name:
            print '<a name="' + self.name + '">'
            print "<h4>" + self.name + "</h4>"
            print "</a>"

        # print source code
        #
        if not self.source:
            print block_footer
            return

        lines = self.source
        l     = len( lines ) - 1
        while l >= 0 and string.strip( lines[l] ) == "":
            l = l - 1
        print source_header
        print ""
        for line in lines[0 : l+1]:
            print html_quote(line)
        print source_footer

        in_table = 0

        # dump each (marker,content) element
        #
        for element in self.items:
            marker  = element[0]
            content = element[1]

            if marker == "description":
                print description_header
                content.dump_html( identifiers )
                print description_footer

            elif not ( marker in types ):
                sys.stdout.write( marker_header )
                sys.stdout.write( marker )
                sys.stdout.write( marker_inter + '\n' )
                content.dump_html( identifiers )
                print marker_footer

        print ""

        print block_footer



#############################################################################
#
# The DocSection class is used to store a given documentation section.
#
# Each section is made of an identifier, an abstract and a description.
#
# For example, look at:
#
#   <Section> Basic_Data_Types
#
#   <Title> FreeType 2 Basic Data Types
#
#   <Abstract>
#      Definitions of basic FreeType data types
#
#   <Description>
#      FreeType defines several basic data types for all its
#      operations...
#
class DocSection:

    def __init__( self, block ):
        self.block       = block
        self.name        = string.lower( block.name )
        self.abstract    = block.find_content( "abstract" )
        self.description = block.find_content( "description" )
        self.elements    = {}
        self.list        = []
        self.filename    = self.name + ".html"
        self.chapter     = None

        # sys.stderr.write( "new section '" + self.name + "'" )


    def add_element( self, block ):
        # check that we don't have a duplicate element in this
        # section
        #
        if self.elements.has_key( block.name ):
            block.print_error( "duplicate element definition for " +
                               "'" + block.name + "' " +
                               "in section " +
                               "'" + self.name + "'\n" +
                               "previous definition in " +
                               "'" + self.elements[block.name].location() + "'" )

        self.elements[block.name] = block
        self.list.append( block )


    def print_warning( self, message ):
        self.block.print_warning( message )


    def print_error( self, message ):
        self.block.print_error( message )


    def dump_html( self, identifiers = None ):
        """make an HTML page from a given DocSection"""

        # print HTML header
        #
        print html_header

        # print title
        #
        print section_title_header
        print self.title
        print section_title_footer

        # print description
        #
        print block_header
        self.description.dump_html( identifiers )
        print block_footer

        # print elements
        #
        for element in self.list:
            element.dump_html( identifiers )

        print html_footer


class DocSectionList:

    def __init__( self ):
        self.sections        = {}    # map section names to section objects
        self.list            = []    # list of sections (in creation order)
        self.current_section = None  # current section
        self.identifiers     = {}    # map identifiers to blocks


    def append_section( self, block ):
        name     = string.lower( block.name )
        abstract = block.find_content( "abstract" )

        if self.sections.has_key( name ):
            # There is already a section with this name in our list.  We
            # will try to complete it.
            #
            section = self.sections[name]
            if section.abstract:
                # This section already has an abstract defined; simply check
                # that the new section doesn't provide a new one.
                #
                if abstract:
                    section.block.print_error(
                      "duplicate section definition for " +
                      "'" + name + "'\n" +
                      "previous definition in " +
                      "'" + section.block.location() + "'\n" +
                      "second definition in " +
                      "'" + block.location() + "'" )
            else:
                # The old section didn't contain an abstract; we are now
                # going to replace it.
                #
                section.abstract    = abstract
                section.description = block.find_content( "description" )
                section.block       = block

        else:
            # a new section
            #
            section = DocSection( block )
            self.sections[name] = section
            self.list.append( section )

        self.current_section = section


    def append_block( self, block ):
        if block.name:
            section = block.find_content( "section" )
            if section:
                self.append_section( block )

            elif self.current_section:
                self.current_section.add_element( block )
                block.section                = self.current_section
                self.identifiers[block.name] = block


    def prepare_files( self, file_prefix = None ):
        # prepare the section list, by computing section filenames and the
        # index
        #
        if file_prefix:
            prefix = file_prefix + "-"
        else:
            prefix = ""

        # compute section names
        #
        for section in self.sections.values():
            title_content     = section.block.find_content( "title" )
            if title_content:
                section.title = title_content.get_title()
            else:
                section.title = "UNKNOWN_SECTION_TITLE!"


        # sort section elements according to the <order> marker if available
        #
        for section in self.sections.values():
            order = section.block.find_content( "order" )
            if order:
                # sys.stderr.write( "<order> found at "
                #                   + section.block.location() + '\n' )
                order_list = []
                for item in order.items:
                    for element in item[1]:
                        words = None
                        try:
                            words = element.get_words()
                        except:
                            section.block.print_warning(
                              "invalid content in <order> marker\n" )
                        if words:
                            for word in words:
                                block = self.identifiers.get( word )
                                if block:
                                    if block.section == section:
                                        order_list.append( block )
                                    else:
                                        section.block.print_warning(
                                          "invalid reference to " +
                                          "'" + word + "' " +
                                          "defined in other section" )
                                else:
                                    section.block.print_warning(
                                      "invalid reference to " +
                                      "'" + word + "'" )

                # now sort the list of blocks according to the order list
                #
                new_list = order_list[:]
                for block in section.list:
                    if not block in order_list:
                        new_list.append( block )

                section.list = new_list

        # compute section filenames
        #
        for section in self.sections.values():
            section.filename = prefix + section.name + ".html"

        self.toc_filename   = prefix + "toc.html"
        self.index_filename = prefix + "index.html"

        # compute the sorted list of identifiers for the index
        #
        self.index = self.identifiers.keys()
        self.index.sort( index_sort )


    def dump_html_sections( self ):
        for section in self.sections.values():
            if section.filename:
                output = open_output( section.filename )

                section.dump_html( self.identifiers )

                close_output( output )


    def dump_html_index( self ):
        output = open_output( self.index_filename )

        num_columns = 3
        total       = len( self.index )
        line        = 0

        print html_header
        print "<center><h1>General Index</h1></center>"
        print "<center><table cellpadding=5><tr valign=top><td>"

        for ident in self.index:
            block = self.identifiers[ident]
            if block:
                sys.stdout.write( '<a href="' + block.html_address() + '">' )
                sys.stdout.write( block.name )
                sys.stdout.write( '</a><br>' + '\n' )

                if line * num_columns >= total:
                    print "</td><td>"
                    line = 0
                else:
                    line = line + 1
            else:
                sys.stderr.write( "identifier '" + ident +
                                  "' has no definition" + '\n' )

        print "</tr></table></center>"
        print html_footer

        close_output( output )



# Filter a given list of DocBlocks.  Returns a new list of DocBlock objects
# that only contains element whose "type" (i.e. first marker) is in the
# "types" parameter.
#
class DocChapter:

    def __init__( self, block ):
        self.sections_names = []    # ordered list of section names
        self.sections       = []    # ordered list of DocSection objects
                                    # for this chapter
        self.block          = block

        # look for chapter title
        content = block.find_content( "title" )
        if content:
            self.title = content.get_title()
        else:
            self.title = "UNKNOWN CHAPTER TITLE"

        # look for section list
        content = block.find_content( "sections" )
        if not content:
            block.print_error( "chapter has no <sections> content" )

        # compute list of section names
        slist = []
        for item in content.items:
            for element in item[1]:
                try:
                    words        = element.get_words()
                    l            = len( slist )
                    slist[l : l] = words
                except:
                    block.print_warning(
                      "invalid content in <sections> marker" )

        self.section_names = slist


class DocDocument:

    def __init__( self ):
        self.section_list  = DocSectionList()   # section list object
        self.chapters      = []                 # list of chapters
        self.lost_sections = []                 # list of sections with
                                                # no chapter

    def append_block( self, block ):
        if block.name:
            content = block.find_content( "chapter" )
            if content:
                # a chapter definition -- add it to our list
                #
                chapter = DocChapter( block )
                self.chapters.append( chapter )
            else:
                self.section_list.append_block( block )


    def prepare_chapters( self ):
        # check section names
        #
        for chapter in self.chapters:
            slist = []
            for name in chapter.section_names:
                 section = self.section_list.sections.get( name )
                 if not section:
                     chapter.block.print_warning(
                       "invalid reference to unknown section '" + name + "'" )
                 else:
                     section.chapter = chapter
                     slist.append( section )

            chapter.sections = slist

        for section in self.section_list.list:
            if not section.chapter:
                section.block.print_warning(
                  "section '" + section.name + "' is not in any chapter" )
                self.lost_sections.append( section )


    def prepare_files( self, file_prefix = None ):
        self.section_list.prepare_files( file_prefix )
        self.prepare_chapters()


    def dump_toc_html( self ):
        # dump an html table of contents
        #
        output = open_output( self.section_list.toc_filename )

        print html_header

        print "<center><h1>Table of Contents</h1></center>"

        for chapter in self.chapters:
            print chapter_header + chapter.title + chapter_inter

            print "<table cellpadding=5>"
            for section in chapter.sections:
                if section.abstract:
                    print "<tr valign=top><td>"
                    sys.stdout.write( '<a href="' + section.filename + '">' )
                    sys.stdout.write( section.title )
                    sys.stdout.write( "</a></td><td>" + '\n' )
                    section.abstract.dump_html( self.section_list.identifiers )
                    print "</td></tr>"

            print "</table>"

            print chapter_footer

        # list lost sections
        #
        if self.lost_sections:
            print chapter_header + "OTHER SECTIONS:" + chapter_inter

            print "<table cellpadding=5>"
            for section in self.lost_sections:
                if section.abstract:
                    print "<tr valign=top><td>"
                    sys.stdout.write( '<a href="' + section.filename + '">' )
                    sys.stdout.write( section.title )
                    sys.stdout.write( "</a></td><td>" + '\n' )
                    section.abstract.dump_html( self.section_list.identifiers )
                    print "</td></tr>"

            print "</table>"

            print chapter_footer

        # index
        #
        print chapter_header + '<a href="' + self.section_list.index_filename + '">Index</a>' + chapter_footer

        print html_footer

        close_output( output )


    def dump_index_html( self ):
        self.section_list.dump_html_index()


    def dump_sections_html( self ):
        self.section_list.dump_html_sections()


def filter_blocks_by_type( block_list, types ):
    new_list = []
    for block in block_list:
        if block.items:
            element = block.items[0]
            marker  = element[0]
            if marker in types:
                new_list.append( block )

    return new_list


def filter_section_blocks( block ):
    return block.section != None


# Perform a lexicographical comparison of two DocBlock objects.  Returns -1,
# 0 or 1.
#
def block_lexicographical_compare( b1, b2 ):
    if not b1.name:
        return -1
    if not b2.name:
        return 1

    id1 = string.lower( b1.name )
    id2 = string.lower( b2.name )

    if id1 < id2:
        return -1
    elif id1 == id2:
        return 0
    else:
        return 1


# Dump a list block as a single HTML page.
#
def dump_html_1( block_list ):
    print html_header

    for block in block_list:
        block.dump_html()

    print html_footer


def file_exists( pathname ):
    result = 1
    try:
        file = open( pathname, "r" )
        file.close()
    except:
        result = None

    return result


def add_new_block( list, filename, lineno, block_lines, source_lines ):
    """add a new block to the list"""
    block          = DocBlock( block_lines, source_lines )
    block.filename = filename
    block.lineno   = lineno
    list.append( block )


def make_block_list( args = None ):
    """parse a file and extract comments blocks from it"""

    file_list = []
    # sys.stderr.write( repr( sys.argv[1 :] ) + '\n' )

    if not args:
        args = sys.argv[1 :]

    for pathname in args:
        if string.find( pathname, '*' ) >= 0:
            newpath = glob.glob( pathname )
            newpath.sort()  # sort files -- this is important because
                            # of the order of files
        else:
            newpath = [pathname]

        last = len( file_list )
        file_list[last : last] = newpath

    if len( file_list ) == 0:
        file_list = None
    else:
        # now filter the file list to remove non-existing ones
        file_list = filter( file_exists, file_list )

    list   = []
    block  = []
    format = 0
    lineno = 0

    # We use "format" to store the state of our parser:
    #
    #  0 - wait for beginning of comment
    #  1 - parse comment format 1
    #  2 - parse comment format 2
    #
    #  4 - wait for beginning of source (or comment?)
    #  5 - process source
    #
    comment = []
    source  = []
    state   = 0

    fileinput.close()
    for line in fileinput.input( file_list ):
        l = len( line )
        if l > 0 and line[l - 1] == '\012':
            line = line[0 : l-1]

        # stripped version of the line
        #
        line2 = string.strip( line )
        l     = len( line2 )

        # if this line begins with a comment and we are processing some
        # source, exit to state 0
        #
        # unless we encounter something like:
        #
        #    /*@.....
        #    /*#.....
        #
        #    /* @.....
        #    /* #.....
        #
        if format >= 4 and l > 2 and line2[0 : 2] == '/*':
            if l < 4 or ( line2[2] != '@' and line2[2 : 4] != ' @' and
                          line2[2] != '#' and line2[2 : 4] != ' #'):
                add_new_block( list, fileinput.filename(),
                               lineno, block, source )
                format = 0

        if format == 0:  #### wait for beginning of comment ####
            if l > 3 and line2[0 : 3] == '/**':
                i = 3
                while i < l and line2[i] == '*':
                    i = i + 1

                if i == l:
                    # this is '/**' followed by any number of '*', the
                    # beginning of a Format 1 block
                    #
                    block  = []
                    source = []
                    format = 1
                    lineno = fileinput.filelineno()

                elif i == l - 1 and line2[i] == '/':
                    # this is '/**' followed by any number of '*', followed
                    # by a '/', i.e. the beginning of a Format 2 or 3 block
                    #
                    block  = []
                    source = []
                    format = 2
                    lineno = fileinput.filelineno()

        ##############################################################
        #
        # FORMAT 1
        #
        elif format == 1:

            # If the line doesn't begin with a "*", something went wrong,
            # and we must exit, and forget the current block.
            #
            if l == 0 or line2[0] != '*':
                block  = []
                format = 0

            # Otherwise, we test for an end of block, which is an arbitrary
            # number of '*', followed by '/'.
            #
            else:
                i = 1
                while i < l and line2[i] == '*':
                    i = i + 1

                # test for the end of the block
                #
                if i < l and line2[i] == '/':
                    if block != []:
                        format = 4
                    else:
                        format = 0
                else:
                    # otherwise simply append line to current block
                    #
                    block.append( line2[i :] )

                continue

        ##############################################################
        #
        # FORMAT 2
        #
        elif format == 2:

            # If the line doesn't begin with '/*' and end with '*/', this is
            # the end of the format 2 format.
            #
            if l < 4 or line2[: 2] != '/*' or line2[-2 :] != '*/':
                if block != []:
                    format = 4
                else:
                    format = 0
            else:
                # remove the start and end comment delimiters, then
                # right-strip the line
                #
                line2 = string.rstrip( line2[2 : -2] )

                # check for end of a format2 block, i.e. a run of '*'
                #
                if string.count( line2, '*' ) == l - 4:
                    if block != []:
                        format = 4
                    else:
                        format = 0
                else:
                    # otherwise, add the line to the current block
                    #
                    block.append( line2 )

                continue

        if format >= 4:  #### source processing ####
            if l > 0:
                format = 5

            if format == 5:
                source.append( line )

    if format >= 4:
        add_new_block( list, fileinput.filename(), lineno, block, source )

    return list



# This function is only used for debugging
#
def dump_block_list( list ):
    """dump a comment block list"""
    for block in list:
        print "----------------------------------------"
        for line in block[0]:
            print line
        for line in block[1]:
            print line

    print "---------the end-----------------------"


def usage():
    print "\nDocMaker 0.1 Usage information\n"
    print "  docmaker [options] file1 [ file2 ... ]\n"
    print "using the following options:\n"
    print "  -h : print this page"
    print "  -t : set project title, as in '-t \"My Project\"'"
    print "  -o : set output directory, as in '-o mydir'"
    print "  -p : set documentation prefix, as in '-p ft2'"
    print ""
    print "  --title  : same as -t, as in '--title=\"My Project\"'"
    print "  --output : same as -o, as in '--output=mydir'"
    print "  --prefix : same as -p, as in '--prefix=ft2'"


def main( argv ):
    """main program loop"""

    global output_dir, project_title, project_prefix
    global html_header, html_header1, html_header2, html_header3

    try:
        opts, args = getopt.getopt( sys.argv[1:],
                                    "ht:o:p:",
                                    [ "help", "title=", "output=", "prefix=" ] )

    except getopt.GetoptError:
        usage()
        sys.exit( 2 )

    if args == []:
        usage()
        sys.exit( 1 )

    # process options
    #
    project_title  = "Project"
    project_prefix = None
    output_dir     = None

    for opt in opts:
        if opt[0] in ( "-h", "--help" ):
            usage()
            sys.exit( 0 )

        if opt[0] in ( "-t", "--title" ):
            project_title = opt[1]

        if opt[0] in ( "-o", "--output" ):
            output_dir = opt[1]

        if opt[0] in ( "-p", "--prefix" ):
            project_prefix = opt[1]

    html_header = html_header_1 + project_title + html_header_2 + project_title + html_header_3
    check_output( )
    compute_time_html()

    # we begin by simply building a list of DocBlock elements
    #
    list = make_block_list( args )

    # now, sort the blocks into sections
    #
    document = DocDocument()
    for block in list:
        document.append_block( block )

    document.prepare_files( project_prefix )

    document.dump_toc_html()
    document.dump_sections_html()
    document.dump_index_html()

# if called from the command line
#
if __name__ == '__main__':
    main( sys.argv )


# eof
