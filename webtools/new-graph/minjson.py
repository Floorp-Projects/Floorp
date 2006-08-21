##############################################################################
##
##    minjson.py implements JSON reading and writing in python.
##    Copyright (c) 2005 Jim Washington and Contributors.
##
##    This library is free software; you can redistribute it and/or
##    modify it under the terms of the GNU Lesser General Public
##    License as published by the Free Software Foundation; either
##    version 2.1 of the License, or (at your option) any later version.
##
##    This library is distributed in the hope that it will be useful,
##    but WITHOUT ANY WARRANTY; without even the implied warranty of
##    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##    Lesser General Public License for more details.=
##
##    You should have received a copy of the GNU Lesser General Public
##    License along with this library; if not, write to the Free Software
##    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
##############################################################################


# minjson.py
# use python's parser to read minimal javascript objects.
# str's objects and fixes the text to write javascript.

# Thanks to Patrick Logan for starting the json-py project and making so many
# good test cases.

# Jim Washington 7 Aug 2005.

from re import compile, sub, search, DOTALL

# set to true if transmission size is much more important than speed
# only affects writing, and makes a minimal difference in output size.
alwaysStripWhiteSpace = False

# add to this string if you wish to exclude additional math operators
# from reading.
badOperators = '*'

#################################
#      read JSON object         #
#################################

slashstarcomment = compile(r'/\*.*?\*/',DOTALL)
doubleslashcomment = compile(r'//.*\n')

def _Read(aString):
        """Use eval in a 'safe' way to turn javascript expression into
           a python expression.  Allow only True, False, and None in global
           __builtins__, and since those map as true, false, null in
           javascript, pass those as locals
        """
        try:
            result = eval(aString,
            {"__builtins__":{'True':True,'False':False,'None':None}},
            {'null':None,'true':True,'false':False})
        except NameError:
            raise ReadException, \
            "Strings must be quoted. Could not read '%s'." % aString
        except SyntaxError:
            raise ReadException, \
            "Syntax error.  Could not read '%s'." % aString
        return result

# badOperators is defined at the top of the module

# generate the regexes for math detection
regexes = {}
for operator in badOperators:
    if operator in '+*':
        # '+' and '*' need to be escaped with \ in re
        regexes[operator,'numeric operation'] \
        = compile(r"\d*\s*\%s|\%s\s*\d*" % (operator, operator))
    else:
        regexes[operator,'numeric operation'] \
        = compile(r"\d*\s*%s|%s\s*\d*" % (operator, operator))

def _getStringState(aSequence):
    """return the list of required quote closures if the end of aString needs them
    to close quotes.
    """
    state = []
    for k in aSequence:
        if k in ['"',"'"]:
            if state and k == state[-1]:
                state.pop()
            else:
                state.append(k)
    return state

def _sanityCheckMath(aString):
    """just need to check that, if there is a math operator in the
       client's JSON, it is inside a quoted string. This is mainly to
       keep client from successfully sending 'D0S'*9**9**9**9...
       Return True if OK, False otherwise
    """
    for operator in badOperators:
        #first check, is it a possible math operation?
        if regexes[(operator,'numeric operation')].search(aString) is not None:
            # OK.  possible math operation. get the operator's locations
            getlocs = regexes[(operator,'numeric operation')].finditer(aString)
            locs = [item.span() for item in getlocs]
            halfStrLen = len(aString) / 2
            #fortunately, this should be rare
            for loc in locs:
                exprStart = loc[0]
                exprEnd = loc[1]
                # We only need to know the char is within open quote
                # status.
                if exprStart <= halfStrLen:
                    teststr = aString[:exprStart]
                else:
                    teststr = list(aString[exprEnd+1:])
                    teststr.reverse()
                if not _getStringState(teststr):
                    return False
    return True

def safeRead(aString):
    """turn the js into happier python and check for bad operations
       before sending it to the interpreter
    """
    # get rid of trailing null. Konqueror appends this, and the python
    # interpreter balks when it is there.
    CHR0 = chr(0)
    while aString.endswith(CHR0):
        aString = aString[:-1]
    # strip leading and trailing whitespace
    aString = aString.strip()
    # zap /* ... */ comments
    aString = slashstarcomment.sub('',aString)
    # zap // comments
    aString = doubleslashcomment.sub('',aString)
    # here, we only check for the * operator as a DOS problem by default;
    # additional operators may be excluded by editing badOperators
    # at the top of the module
    if _sanityCheckMath(aString):
        return _Read(aString)
    else:
        raise ReadException, 'Unacceptable JSON expression: %s' % aString

read = safeRead

#################################
#   write object as JSON        #
#################################

#alwaysStripWhiteSpace is defined at the top of the module

tfnTuple = (('True','true'),('False','false'),('None','null'),)

def _replaceTrueFalseNone(aString):
    """replace True, False, and None with javascript counterparts"""
    for k in tfnTuple:
        if k[0] in aString:
            aString = aString.replace(k[0],k[1])
    return aString

def _handleCode(subStr,stripWhiteSpace):
    """replace True, False, and None with javascript counterparts if
       appropriate, remove unicode u's, fix long L's, make tuples
       lists, and strip white space if requested
    """
    if 'e' in subStr:
        #True, False, and None have 'e' in them. :)
        subStr = (_replaceTrueFalseNone(subStr))
    if stripWhiteSpace:
        # re.sub might do a better job, but takes longer.
        # Spaces are the majority of the whitespace, anyway...
        subStr = subStr.replace(' ','')
    if subStr[-1] in "uU":
        #remove unicode u's
        subStr = subStr[:-1]
    if "L" in subStr:
        #remove Ls from long ints
        subStr = subStr.replace("L",'')
    #do tuples as lists
    if "(" in subStr:
        subStr = subStr.replace("(",'[')
    if ")" in subStr:
        subStr = subStr.replace(")",']')
    return subStr

# re for a double-quoted string that has a single-quote in it
# but no double-quotes and python punctuation after:
redoublequotedstring = compile(r'"[^"]*\'[^"]*"[,\]\}:\)]')
escapedSingleQuote = r"\'"
escapedDoubleQuote = r'\"'

def doQuotesSwapping(aString):
    """rewrite doublequoted strings with single quotes as singlequoted strings with
    escaped single quotes"""
    s = []
    foundlocs = redoublequotedstring.finditer(aString)
    prevend = 0
    for loc in foundlocs:
        start,end = loc.span()
        s.append(aString[prevend:start])
        tempstr = aString[start:end]
        endchar = tempstr[-1]
        ts1 = tempstr[1:-2]
        ts1 = ts1.replace("'",escapedSingleQuote)
        ts1 = "'%s'%s" % (ts1,endchar)
        s.append(ts1)
        prevend = end
    s.append(aString[prevend:])
    return ''.join(s)

def _pyexpr2jsexpr(aString, stripWhiteSpace):
    """Take advantage of python's formatting of string representations of
    objects.  Python always uses "'" to delimit strings.  Except it doesn't when
    there is ' in the string.  Fix that, then, if we split
    on that delimiter, we have a list that alternates non-string text with
    string text.  Since string text is already properly escaped, we
    only need to replace True, False, and None in non-string text and
    remove any unicode 'u's preceding string values.

    if stripWhiteSpace is True, remove spaces, etc from the non-string
    text.
    """
    inSingleQuote = False
    inDoubleQuote = False
    #python will quote with " when there is a ' in the string,
    #so fix that first
    if redoublequotedstring.search(aString):
        aString = doQuotesSwapping(aString)
    marker = None
    if escapedSingleQuote in aString:
        #replace escaped single quotes with a marker
        marker = markerBase = '|'
        markerCount = 1
        while marker in aString:
            #if the marker is already there, make it different
            markerCount += 1
            marker = markerBase * markerCount
        aString = aString.replace(escapedSingleQuote,marker)

    #escape double-quotes
    aString = aString.replace('"',escapedDoubleQuote)
    #split the string on the real single-quotes
    splitStr = aString.split("'")
    outList = []
    alt = True
    for subStr in splitStr:
        #if alt is True, non-string; do replacements
        if alt:
            subStr = _handleCode(subStr,stripWhiteSpace)
        outList.append(subStr)
        alt = not alt
    result = '"'.join(outList)
    if marker:
        #put the escaped single-quotes back as "'"
        result = result.replace(marker,"'")
    return result

def write(obj, encoding="utf-8",stripWhiteSpace=alwaysStripWhiteSpace):
    """Represent the object as a string.  Do any necessary fix-ups
    with pyexpr2jsexpr"""
    try:
        #not really sure encode does anything here
        aString = str(obj).encode(encoding)
    except UnicodeEncodeError:
        aString = obj.encode(encoding)
    if isinstance(obj,basestring):
        if '"' in aString:
            aString = aString.replace(escapedDoubleQuote,'"')
            result = '"%s"' % aString.replace('"',escapedDoubleQuote)
        else:
            result = '"%s"' % aString
    else:
        result = _pyexpr2jsexpr(aString,stripWhiteSpace).encode(encoding)
    return result

class ReadException(Exception):
    pass

class WriteException(Exception):
    pass
