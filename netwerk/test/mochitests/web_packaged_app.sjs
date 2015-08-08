var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "application/package", false);
  response.write(octetStreamData.getData());
  return;
}

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var octetStreamData = {
  content: [
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script> alert('OK: hello'); alert('DONE'); </script>\r\n</head>\r\n  Web Packaged App Index\r\n</html>\r\n", type: "text/html" },
   { headers: ["Content-Location: /scripts/app.js", "Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
   { headers: ["Content-Location: /scripts/helpers/math.js", "Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
  ],
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";
    for (var i in this.content) {
      str += "--" + this.token + "\r\n";
      for (var j in this.content[i].headers) {
        str += this.content[i].headers[j] + "\r\n";
      }
      str += "\r\n";
      str += this.content[i].data + "\r\n";
    }

    str += "--" + this.token + "--";
    return str;
  }
}
