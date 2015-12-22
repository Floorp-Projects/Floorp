// Same as signed_web_packaged_app.sjs except this one would return a random
// package-identifer.

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

var uuid = Cc["@mozilla.org/uuid-generator;1"].
             getService(Ci.nsIUUIDGenerator).
             generateUUID().
             toString().replace(/[{}]/g, "");

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "application/package", false);
  response.write(signedPackage);
  return;
}

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var signedPackage = `manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgECMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MDkxMDA4MDQzNVoXDTM1MDkxMDA4MDQzNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAts8whjOzEbn/w1xkFJ67af7F/JPujBK91oyJekh2schIMzFau9pY8S1AiJQoJCulOJCJfUc8hBLKBZiGAkii+4Gpx6cVqMLe6C22MdD806Soxn8Dg4dQqbIvPuI4eeVKu5CEk80PW/BaFMmRvRHO62C7PILuH6yZeGHC4P7dTKpsk4CLxh/jRGXLC8jV2BCW0X+3BMbHBg53NoI9s1Gs7KGYnfOHbBP5wEFAa00RjHnubUaCdEBlC8Kl4X7p0S4RGb3rsB08wgFe9EmSZHIgcIm+SuVo7N4qqbI85qo2ulU6J8NN7ZtgMPHzrMhzgAgf/KnqPqwDIxnNmRNJmHTUYwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAukH6cJUUj5faa8CuPCqrEa0PoLY4SYNnff9NI+TTAHkB9l+kOcFl5eo2EQOcWmZKYi7QLlWC4jy/KQYattO9FMaxiOQL4FAc6ZIbNyfwWBzZWyr5syYJTTTnkLq8A9pCKarN49+FqhJseycU+8EhJEJyP5pv5hLvDNTTHOQ6SXhASsiX8cjo3AY4bxA5pWeXuTZ459qDxOnQd+GrOe4dIeqflk0hA2xYKe3SfF+QlK8EO370B8Dj8RX230OATM1E3OtYyALe34KW3wM9Qm9rb0eViDnVyDiCWkhhQnw5yPg/XQfloug2itRYuCnfUoRt8xfeHgwz2Ymz8cUADn3KpTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQIwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTAyODExMTIwMlowIwYJKoZIhvcNAQkEMRYEFENKTXRUkdej+EPd/oKRhz0Cp13zMA0GCSqGSIb3DQEBAQUABIIBAFCr+i8cwTiwzzCVjzZZI2NAqu8dnYOAJjkhD02tJjBCbvehEhXW6pP/Gk8+oyx2zoV87zbw9xBGcEU9b3ulbggdFR56S3C3w+eTbeOXMcx7A8mn9vvsoMJm+/rkT4DgEUU1iaM7pdwH48CKJOnAZP5FkjRvpRBh8TgfcDbusXveYTwG5LVpDp8856+9FBzvZ7wLz9iWDvlT/EFxfWOnGduAJunQ9qQm+pWu5cvSTwWasCMYmiPRlsuBhU9Fx7LtlXIHtE2nYYQVMTMDE58z/mzT34W0bnneecrghHREhb90UvdlUZJ2q3Jahsa3718WUGPTp7ZYwYaPBy7ryoOoWSA=\r
--7IYGY9UDJB\r
Content-Location: manifest.webapp\r
Content-Type: application/x-web-app-manifest+json\r
\r
{
  "moz-package-origin": "http://mochi.test:8888",
  "name": "My App",
  "moz-resources": [
    {
      "src": "page2.html",
      "integrity": "JREF3JbXGvZ+I1KHtoz3f46ZkeIPrvXtG4VyFQrJ7II="
    },
    {
      "src": "index.html",
      "integrity": "IjQ2S/V9qsC7wW5uv/Niq40M1aivvqH5+1GKRwUnyRg="
    },
    {
      "src": "scripts/script.js",
      "integrity": "6TqtNArQKrrsXEQWu3D9ZD8xvDRIkhyV6zVdTcmsT5Q="
    },
    {
      "src": "scripts/library.js",
      "integrity": "TN2ByXZiaBiBCvS4MeZ02UyNi44vED+KjdjLInUl4o8="
    }
  ],
  "moz-permissions": [
    {
      "systemXHR": {
        "description": "Needed to download stuff"
      },
      "devicestorage:pictures": {
        "description": "Need to load pictures"
      }
    }
  ],

`
  + '  "package-identifier": "' +  uuid + '",\n\r' +
`
  "description": "A great app!"
}\r
--7IYGY9UDJB\r
Content-Location: page2.html\r
Content-Type: text/html\r
\r
<html>
  page2.html
</html>
\r
--7IYGY9UDJB\r
Content-Location: index.html\r
Content-Type: text/html\r
\r
<html>
  Last updated: 2015/10/28
</html>
\r
--7IYGY9UDJB\r
Content-Location: scripts/script.js\r
Content-Type: text/javascript\r
\r
// script.js
\r
--7IYGY9UDJB\r
Content-Location: scripts/library.js\r
Content-Type: text/javascript\r
\r
// library.js
\r
--7IYGY9UDJB--`;
