var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "application/package", false);
  response.write(signedPackage);
}

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var signedPackage = `manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgEEMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MTExOTAzMDEwNVoXDTM1MTExOTAzMDEwNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzPback9X7RRxKTc3/5o2vm9Ro6XNiSM9NPsN3djjCIVz50bY0rJkP98zsqpFjnLwqHeJigxyYoqFexRhRLgKrG10CxNl4rxP6CEPENjvj5FfbX/HUZpT/DelNR18F498yD95vSHcSrCc3JrjV3bKA+wgt11E4a0Ba95S1RuwtehZw1+Y4hO8nHpbSGfjD0BpluFY2nDoYAm+aWSrsmLuJsKLO8Xn2I1brZFJUynR3q1ujuDE9EJk1niDLfOeVgXM4AavJS5C0ZBHhAhR2W+K9NN97jpkpmHFqecTwDXB7rEhsyB3185cI7anaaQfHHfH5+4SD+cMDNtYIOSgLO06ZwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAlnVyLz5dPhS0ZhZD6qJOUzSo6nFwMxNX1m0oS37mevtuh0b0o1gmEuMw3mVxiAVkC2vPsoxBL2wLlAkcEdBPxGEqhBmtiBY3F3DgvEkf+/sOY1rnr6O1qLZuBAnPzA1Vnco8Jwf0DYF0PxaRd8yT5XSl5qGpM2DItEldZwuKKaL94UEgIeC2c+Uv/IOyrv+EyftX96vcmRwr8ghPFLQ+36H5nuAKEpDD170EvfWl1zs0dUPiqSI6l+hy5V14gl63Woi34L727+FKx8oatbyZtdvbeeOmenfTLifLomnZdx+3WMLkp3TLlHa5xDLwifvZtBP2d3c6zHp7gdrGU1u2WTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQQwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTEyNTAzMDQzMFowIwYJKoZIhvcNAQkEMRYEFD4ut4oKoYdcGzyfQE6ROeazv+uNMA0GCSqGSIb3DQEBAQUABIIBAFG99dKBSOzQmYVn6lHKWERVDtYXbDTIVF957ID8YH9B5unlX/PdludTNbP5dzn8GWQV08tNRgoXQ5sgxjifHunrpaR1WiR6XqvwOCBeA5NB688jxGNxth6zg6fCGFaynsYMX3FlglfIW+AYwyQUclbv+C4UORJpBjvuknOnK+UDBLVSoP9ivL6KhylYna3oFcs0SMsumc/jf/oQW51LzFHpn61TRUqdDgvGhwcjgphMhKj23KwkjwRspU2oIWNRAuhZgqDD5BJlNniCr9X5Hx1dW6tIVISO91CLAryYkGZKRJYekXctCpIvldUkIDeh2tAw5owr0jtsVd6ovFF3bV4=\r
--NKWXJUAFXB\r
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
      "integrity": "Jkvco7U8WOY9s0YREsPouX+DWK7FWlgZwA0iYYSrb7Q="
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
  "package-identifier": "09bc9714-7ab6-4320-9d20-fde4c237522c",
  "description": "A great app!"
}\r
--NKWXJUAFXB\r
Content-Location: page2.html\r
Content-Type: text/html\r
\r
<html>
  page2.html
</html>
\r
--NKWXJUAFXB\r
Content-Location: index.html\r
Content-Type: text/html\r
\r
<html>
  Last updated: 2015/10/28
  <iframe id="innerFrame" src="page2.html"></iframe>
</html>
\r
--NKWXJUAFXB\r
Content-Location: scripts/script.js\r
Content-Type: text/javascript\r
\r
// script.js
\r
--NKWXJUAFXB\r
Content-Location: scripts/library.js\r
Content-Type: text/javascript\r
\r
// library.js
\r
--NKWXJUAFXB--`;
