function writeCurrentDictionary()
{
    var q = document.location.search.substring(1);
    var paramStrings = q.split("&");
    var params = { };
    for (var i = 0; i < paramStrings.length; i++) {
        var asunder = paramStrings[i].split("=");
        params[asunder[0]] = asunder[1];
    }
    var lang = params.lang || 'en-US';

    if (!(lang in allDictionaries)) {
        // Try just the language without region.
        lang = lang.replace(/-.*/, "");
        if (!(lang in allDictionaries))
            return; // alas, nothing to be done...
    }

    dict = allDictionaries[lang];
    document.write("<div class='corner-box'>Install dictionary");
    if (dict.size)
        document.write(" (", dict.size, "KB)");
    document.write("<div class='install-button'><a href='",
                   dict.link, "'><span>", dict.entry.name);
    if (dict.entry.localName != dict.entry.name)
        document.write(" / ", dict.entry.localName);
    document.writeln("</span></a>");
    document.writeln("</div></div>");       
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

var allDictionariesOrdered = [];
var allDictionaries = { };

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
    var dict = {code: code, link: link, size: size, entry: region};
    allDictionariesOrdered.push(dict);
    allDictionaries[code] = dict;
}

function writeAllDictionaries()
{
    for (var i = 0; i < allDictionariesOrdered.length; i++) {
        var dict = allDictionariesOrdered[i];
        var alt = i % 2 ? "even" : "odd";
        document.write("<tr class='", alt, "'>");
        document.write("<td class='left'>", dict.entry.name, "</td>");
        document.write("<td>", dict.entry.localName, "</td>");
        document.write("<td class='right'><a href='", dict.link,
                       "'>Install</a> (", dict.size, " KB)</td></tr>");
/*
        document.writeln("<tr id='", dict.code, "'><td><a href='", dict.link,
                         "' title='", dict.code, "'>", dict.entry.name, '<br>',
                         dict.entry.localName, "</a> </td><td>", dict.size, "KB</td></tr>");
*/
    }
}