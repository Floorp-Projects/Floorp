/* An adapted version of sign.cmd from NSS */
PARSE ARG dist filename
dist=forwardtoback(dist);
'@echo 'dist
'set BEGINLIBPATH='dist'\bin;%BEGINLIBPATH%'
'set LIBPATHSTRICT=T'
dist'\bin\shlibsign -v -i 'filename
exit

forwardtoback: procedure
  arg pathname
  parse var pathname pathname'/'rest
  do while (rest <> "")
    pathname = pathname'\'rest
    parse var pathname pathname'/'rest
  end
  return pathname
