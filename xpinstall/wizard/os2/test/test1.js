////////////////////////////////////////////////////////////////////////////////////////
// test1.js
// 
// Acceptance test 1
// 	 Adds a component to the testxpi directory
//
//   XPInstall test
//   Feb 8, 2000
//
////////////////////////////////////////////////////////////////////////////////////////

var regName = "testxpi test 1";
var vi      = "1.0.1.9"
var jarSrc  = "test1.txt";
var err;

err = startInstall("Acceptance: test1", "install test file 1", vi, 0);
logComment("startInstall() returned: " + err);

f   = getFolder("Program", "testxpi");
logComment("getFolder() returned: " + f);

err = addFile(regName, vi, jarSrc, f, jarSrc, true);
logComment("addFile() returned: " + err);


err = getLastError();
if(0 == err)
{
	err = finalizeInstall();
  logComment("finalizeInstall() returned: " + err);
}
else
{
	abortInstall(err);
}
