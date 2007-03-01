initInstall("penelope", "/mozilla/penelope/", ""); 

var chromeDir = getFolder("Profile", "chrome");

addFile("penelope","chrome/penelope.jar",chromeDir,"");
/* addFile("penelope-service","components/penelope-service.js",getFolder("Program","components"),""); */

registerChrome(PACKAGE | PROFILE_CHROME, getFolder(chromeDir,"penelope.jar"), "content/");
registerChrome(SKIN | PROFILE_CHROME, getFolder(chromeDir, "penelope.jar"), "skin/modern/penelope/");


function doLocale(a)
{
    registerChrome(LOCALE | PROFILE_CHROME, getFolder(chromeDir,"penelope.jar"), "locale/"+a);
}

doLocale("en-US/");

if (0 == getLastError())
{
    performInstall();
}
else
{
    cancelInstall();
}
