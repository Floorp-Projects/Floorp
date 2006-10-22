function writeCurrentDictionary()
{
   document.writeln("<div class='corner-box'>Man, there is so going to be a special link here!</div>");
}

function regionForCode(code)
{
    var AB_CD = code.split("-");
    var AB = AB_CD[0].toLowerCase();
    var CD = (AB_CD.length > 1 ? AB_CD[1].toLowerCase() : "__");

    var lang = gLanguages[AB];
    if (!lang)
        return {name: code, localName: code};
    var region = lang[CD];
    if (region)
        return region;

    /* If we don't have the right region, use the general form
     * and append the language code to disambiguate.
     *
     * We take advantage of the regularity of the data here -- general
     * language name followed optionally by parenthetical region -- so
     * please mind your step.
     */
    for (var any in lang) { // any child will do
        region = lang[any];
        var name = region.name.replace(/ \(.*/, "");
        var localName = region.localName.replace(/ \(.*/, "") + " (" + code + ")";
        return {name: name, localName: localName};
    }
    
    // Absolute last resort.
    return { name: code, localName: code };
}

function addDictionary(code, link, size)
{
    // Sometimes, there is more than one code in the name, yay
    if (code.indexOf("_") != -1) {
        var codes = code.split("_");
        for (var i = 0; i < codes.length; i++) {
            addDictionary(codes[i], link, size);
        }
        return;
    }

    var region = regionForCode(code);
    document.writeln("<tr id='", code, "'><td><a href='", link, "' title='", code, "'>", region.name, ' / ',
                     region.localName, "</td></tr>");
}