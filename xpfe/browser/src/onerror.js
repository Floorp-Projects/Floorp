function onerror(msg, URL, lineNum) {
var errWind = window.open("","errors","HEIGHT=270,WIDTH=400");
var wintxt = "<HTML><BODY BGCOLOR=RED>";
wintxt += "<B>An error has occurred on this page.</B>";

wintxt += "<FORM METHOD=POST ENCTYPE='text-plain'>";

wintxt += "<TEXTAREA COLS=45 ROWS=8 WRAP=VIRTUAL>";
wintxt += "Error: " + msg + "\n";
wintxt += "URL: " + URL + "\n";
wintxt += "Line: " + lineNum + "\n";
wintxt += "Client: " + navigator.userAgent + "\n";
wintxt += "-----------------------------------------\n";
wintxt += "</TEXTAREA><P>";
wintxt += "<INPUT TYPE=button VALUE='Close' onClick='self.close()'>";
wintxt += "</FORM></BODY></HTML>";
errWind.document.write(wintxt);
errWind.document.close();
return true;
}