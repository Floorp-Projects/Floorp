function testsvg() {
    var isOpera = (navigator.userAgent.toLowerCase().indexOf("opera") != -1);
    var ieVersion = navigator.appVersion.match(/MSIE (\d\.\d)/);
    var safariVersion = navigator.userAgent.match(/AppleWebKit\/(\d+)/);
    var operaVersion = navigator.userAgent.match(/Opera\/(\d*\.\d*)/);
    var mozillaVersion = navigator.userAgent.match(/rv:(\d*\.\d*).*Gecko/);
    

    if (ieVersion && (ieVersion[1] >= 6) && !isOpera) {
        var dummysvg = document.createElement('<svg:svg width="1" height="1" baseProfile="full" version="1.1" id="dummy">');
        try {
            dummysvg.getSVGDocument();
            dummysvg = null;
            return true;
        }
        catch (e) {
            return false;
        }
    }
    
    if (safariVersion && (safariVersion[1] > 419))
        return true;

    if (operaVersion && (operaVersion[1] > 8.9))
        return true
    
    if (mozillaVersion && (mozillaVersion > 1.7))
        return true;
    
    return false;
}