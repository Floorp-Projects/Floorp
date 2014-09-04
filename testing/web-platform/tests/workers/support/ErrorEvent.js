onmessage = function(evt)
{
    throw(evt.data);
}

onerror = function(message, location, line, col)
{
    postMessage( {"message": message, "filename": location, "lineno": line, "colno": col} );
    return true;
}