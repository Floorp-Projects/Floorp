' Opens a URL and POST data


sPostData = "name1=value1"
OutputString "CGI script POST test to URL " + TestCGI + sPostData
WebBrowser.Navigate TestCGI, 0, , sPostData
