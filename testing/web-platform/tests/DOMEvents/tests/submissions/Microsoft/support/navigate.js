var docToLoad;
var arrDocs = [
               "createEvent.NOT_SUPPORTED_ERR.html","dispatchEvent.NOT_SUPPORTED_ERR.html","dispatchEvent.UNSPECIFIED_EVENT_TYPE_ERR.html",
               "dispatchEvent.click.checkbox.html", "MouseEvent.image.map.area.html","TextEvent.hasFeature.html","TextEvent.initTextEvent.html",
               "TextEvent.inputMode.drop.html","TextEvent.inputMode.IME.html","TextEvent.inputMode.keyboard.html","TextEvent.inputMode.paste.html",
               "TextEvent.inputMode.script.html","WheelEvent.hasFeature.html","WheelEvent.initWheelEvent.html","WheelEvent.ctrlKey.zoom.html","WheelEvent.deltaMode.html",
               "MutationEvent.hasFeature.html","MutationEvent.initMutationEvent.html",
               "DOMCharacterDataModified.html","DOMNodeInserted.html","DOMNodeRemoved.html","DOMSubtreeModified.html","MutationEvent.relatedNode.html",
               "WheelEvent.preventDefault.scroll.html","MouseEvent.preventDefault.html","UIEvent.load.stylesheet.html",
               "abort.img.html","blur.html","customevent.html","error.image.html","event.defaultprevented.html","Event.eventPhase.html",
               "Event.stopPropagation.html","focusin.html","KeyboardEvent.key.html","load.image.html","CompositionEvent.html",
               "compositionstart.data.html","compositionstart.keydown.html","compositionstart.preventDefault.html",
               "DOMAttrModified.html","DOMAttrModified.attrChange.html","DOMAttrModified.attrName.html","DOMAttrModified.newValue.html",
               "DOMAttrModified.prevValue.html","DOMAttrModified.relatedNode.html","hasFeature.Events.html","hasFeature.feature.string.html",
               "focusin.relatedTarget.html","focusout.relatedTarget.html","mouseenter.relatedTarget.html","mouseleave.relatedTarget.html",
               "MouseEvent.button.html","mouseenter.ctrlKey.html","KeyboardEvent.location.html","KeyboardEvent.modifiers.html"
              ];

var currentDoc;

function parseVars()
{
    var query = window.location.href.toString(); 
    var fullurl = query.split("?");
    if(null != fullurl[1])
    {
        var vars = fullurl[1].split("&");

        for(i = 0; i < vars.length; i++)
        {
            var pair = vars[i].split("=");
            if(pair[0] == "url")
            {
                docToLoad = pair[1];
            }
        }
    }
}

function updateTestInfo(iframeToLoad)
{
    var iframeTitle = document.getElementById("testTitle");
    iframeTitle.value = iframeToLoad.src;

    var externalLink = document.getElementById("testLink");
    externalLink.href = iframeToLoad.src;

    var testCount = document.getElementById("testCount");
    testCount.value = (currentDoc + 1 + "/" + arrDocs.length + " tests");
}

function loadPage()
{
    parseVars();

    var iframeToLoad = window.document.getElementById("testFrame");
    if(docToLoad != null)
    {
        iframeToLoad.src = "./" + docToLoad;
        currentDoc = findCurrentDoc();
    }
    else
    {
        iframeToLoad.src = "./" + arrDocs[0].toString();
        currentDoc = 0;
    }

    updateTestInfo(iframeToLoad);
}

function findCurrentDoc()
{
    for(i = 0; i < arrDocs.length; i++)
    {
        if(docToLoad == arrDocs[i])
        {
            return i;
        }
    }
}

function loadNext()
{
    if(currentDoc < arrDocs.length - 1)
    {
        var iframeToLoad = window.document.getElementById("testFrame");
        iframeToLoad.src = "./" + arrDocs[++currentDoc].toString();
         
        updateTestInfo(iframeToLoad);
    }
    else
    {
        alert("End of Suite");
    }
}

function loadPrev()
{
    if(currentDoc > 0)
    {
        var iframeToLoad = window.document.getElementById("testFrame");
        iframeToLoad.src = "./" + arrDocs[--currentDoc].toString();
         
        updateTestInfo(iframeToLoad);
    }
    else
    {
        alert("Beginning of Suite");
    }
}