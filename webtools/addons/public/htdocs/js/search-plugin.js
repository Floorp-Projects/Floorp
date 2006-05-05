function addEngine(name,ext,cat,type)
{
    if ((typeof window.sidebar == "object") && (typeof window.sidebar.addSearchEngine == "function")) { 
        window.sidebar.addSearchEngine(
            "http://addons.mozilla.org/search-engines-static/"+name+".src",
            "http://addons.mozilla.org/search-engines-static/"+name+"."+ext, name, cat
        );
    } else {
        alert("Sorry, you need a Mozilla-based browser to install a search plugin.");
    } 
}
