/* Equivalent to sign.sh for OS/2 */
PARSE ARG dist objdir iswindows therest
dist=forwardtoback(dist);
objdir=forwardtoback(objdir);
'echo 'dist
'echo 'objdir
'set BEGINLIBPATH='dist'\lib'
objdir'\shlibsign -v -i 'therest
exit

forwardtoback: procedure
  arg pathname
  parse var pathname pathname'/'rest
  do while (rest <> "")
    pathname = pathname'\'rest
    parse var pathname pathname'/'rest
  end
  return pathname
