rem This batch file simply runs the viewer with appropriate
rem command line arguments to cause it to visit a series of
rem urls (specified in url.txt) and allows 15 seconds for each
rem to load. 
rem the output is put into smoke_status.txt. This is where you 
rem will find if it successfully finished loading the various
rem pages in the allotted 15 seconds.
rem if the url.txt gets too large, the log file will suck. 
rem keep that in mind. If it looks like it's getting unmanagable, 
rem Bindu has some alternative filters to run it through to weed
rem out the interesting information. She can be contacted at 
rem bsharma@netscape.com

@echo off
y:\recover\mozilla\dist\WIN32_D.OBJ\bin\viewer.exe -v -d 15 -f y:\url.txt > y:\smoke.status.txt



