var httpHostMain = 'w3c-test.org'; //name of the server that this page must accessed over port 80
var httpHostAlias = 'www.w3c-test.org'; //another hostname (must be a subdomain so document.domain can be set to a higher domain) that accesses the same content, over HTTP
var httpsHostAlias = httpHostAlias; //another hostname (can be same as httpHostAlias) that accesses the same content, over HTTPS port 
var httpPortAlias = 81; //another port that accesses the same content on the current hostname, over HTTP
var httpsPortAlias = 8443; //another port that accesses the same content on the httpsHostAlias, over HTTPS

function crossOriginUrl(subdomain, relative_url) {
  var a = document.createElement("a");
  a.href = relative_url;
  return a.href.replace(location.href.replace("://", "://" + subdomain + "."));
}