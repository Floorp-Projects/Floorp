onmessage = function(evt)
{
    var url = evt.data;
    
    try
    {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, false);
        xhr.send();
        postMessage(xhr.responseText);
    }
    catch (ex)
    {
        postMessage(ex.toString());
    }
}