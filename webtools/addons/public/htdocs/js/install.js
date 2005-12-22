function install( aEvent, extName, iconURL)  {   
    var p = new XMLHttpRequest();
    p.open("GET", "/core/install.php?uri="+aEvent.target.href, false);
    p.send(null);

    var params = new Array();
    params[extName] = {
        URL: aEvent.target.href,
        IconURL: iconURL,
        toString: function () { return this.URL; }
    };
    InstallTrigger.install(params);
    return false;
}
