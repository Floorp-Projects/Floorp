/* Invoke unit tests on OS/2 */
PARSE ARG dist prog parm
dist=forwardtoback(dist);
'set BEGINLIBPATH='dist'\bin;%BEGINLIBPATH%'
'set LIBPATHSTRICT=T'
if substr(FILESPEC("name",prog),1,5) \= 'test_'
     then
	do
           prog=forwardtoback(prog)
           prog parm
           exit
        end
     else
   bash prog parm
exit

forwardtoback: procedure
  arg pathname
  parse var pathname pathname'/'rest
  do while (rest <> "")
    pathname = pathname'\'rest
    parse var pathname pathname'/'rest
  end
  return pathname
