function getUsername() 
{
    return readCookie('amo_user');
}

function addUsernameToHeader() 
{
    var username = getUsername();
    
    if (username && username.length > 0) {
    
        document.writeln('<div id="auth">Logged in as: ' + htmlEntities(username) + '</div>');

    }
}

/* From http://www.quirksmode.org/js/cookies.html */
function readCookie(name)
{
    var nameEQ = name + "=";
    var ca = document.cookie.split(';');
    for(var i=0;i < ca.length;i++)
    {
        var c = ca[i];
        while (c.charAt(0)==' ') c = c.substring(1,c.length);
        if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
    }
    return null;
}

/* Someone should make this a js builtin */
function htmlEntities(str)
{
    return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/\+/g,'&nbsp;');
}
