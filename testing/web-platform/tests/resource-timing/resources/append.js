function appendScript(src) {
    const script = document.createElement('script');
    script.type = 'text/javascript';
    script.src = src;
    document.body.appendChild(script);
}

function xhrScript(src) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", src, false);
    xhr.send(null);
}
