////////////////////////////////////////////////////////////////////////////////////////
// test2.js
// 
// Acceptance test 2
// 	 Adds a component to the testxpi directory
//
//   XPInstall test
//   Feb 8, 2000
//
////////////////////////////////////////////////////////////////////////////////////////

var regName = "testxpi test 2";
var vi      = "2.0.1.9"
var jarSrc  = "test2.txt";
var err;

err = startInstall("Acceptance: test2", "install test file 2", vi, 0);
logComment("startInstall() returned: " + err);

f   = getFolder("Program", "testxpi");
logComment("getFolder() returned: " + f);

err = addFile(regName, vi, jarSrc, f, jarSrc, true);
logComment("addFile() returned: " + err);

err == getLastError();
if(0 == err)
{
	err = finalizeInstall();
  logComment("finalizeInstall() returned: " + err);
}
else
{
	abortInstall(err);
}
