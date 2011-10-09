let fennecID = "{a23983c0-fd0e-11dc-95ff-0800200c9a66}";
Components.utils.import("resource://gre/modules/Services.jsm");

function getLocale(aLocaleParams, aAppParams) {
 let l = {
  TARGETLOCALE: "test{IDNUMBER}",
  NAME: "Test Locale {IDNUMBER}",
  VERSION: "1.0",
  INSTALL: "http://www.example.com/browser/mobile/chrome/tests/addons/browser_locale{IDNUMBER}.xpi",
  TYPENUMBER: 5,
  TYPENAME: "Language Pack (Application)",
  IDNUMBER: "",
 };
 let a = {
  APPNAME: "Fennec",
  MINVERSION: "4.0", MAXVERSION: "*",
  APPID: fennecID
 };

 if (aLocaleParams) {
  for (var entry in aLocaleParams) {
   l[entry] = aLocaleParams[entry];
  }
 }

 if (aAppParams) {
  for (var entry in aAppParams) {
   a[entry] = aAppParams[entry];
  }
 }

 l.app = a;
 return l;
}

let appTemplate = "<application>" +
"<name>{APPNAME}</name>" +
 "<min_version>{MINVERSION}</min_version>" +
 "<max_version>{MAXVERSION}</max_version>" +
 "<appID>{APPID}</appID>" +
"</application>";

let template = "<addon>"+
 "<target_locale>{TARGETLOCALE}</target_locale>" +
 "<name>{NAME}</name>"+
 "<type id=\"{TYPENUMBER}\">{TYPENAME}</type>"+
 "<guid>langpack-{TARGETLOCALE}@firefox-mobile.mozilla.org</guid>"+
 "<version>{VERSION}</version>"+
 "<status id=\"4\">Public</status>"+
 "<compatible_applications>{APPS}</compatible_applications>"+
 "<all_compatible_os><os>ALL</os></all_compatible_os>"+
 "<install os=\"ALL\">{INSTALL}</install><strings>\n" +
  "title=TITLE\n" +
  "continueIn=CONTINUEIN%S\n" +
  "name=NAME\n" +
  "choose=CHOOSE\n" +
  "chooseLanguage=CHOOSELANGUAGE\n" +
  "cancel=CANCEL\n" +
  "continue=CONTINUE\n" +
  "installing=INSTALLING%S\n" +
  "installerror=INSTALLERROR\n" +
  "loading=LOADING" +
 "</strings>"+
"</addon>";

function handleRequest(request, response) {

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/xml", false);

  response.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  response.write("<addons>");

  let locales = [];
  let query = decodeURIComponent(request.queryString || "").split("=");
  switch(query[0]) {
   case "numvalid":
    let numValid = parseInt(query[1]);
    for (let i = 0; i < numValid; i++) {
     locales.push( getLocale({IDNUMBER: i}) );
    }
    break;
   case "buildid":
    locales.push( getLocale({IDNUMBER: 1, NAME: query[1]}) );
    break;
   default :
    locales.push( getLocale({IDNUMBER: 1}) );
    /* These locales should fail in the LocaleRepository */
    locales.push( getLocale({IDNUMBER: 1}) ); // no duplicate ids
    locales.push( getLocale({IDNUMBER: 2, INSTALL: "INVALID_URL"}) );
    locales.push( getLocale({IDNUMBER: 3}, {APPID: "INVALID_ID"}) );
    locales.push( getLocale({IDNUMBER: 3}, {MAXVERSION: "0"}) );
    locales.push( getLocale({IDNUMBER: 4, TARGETLOCALE: ""}) );
    locales.push( getLocale({IDNUMBER: 5, NAME: ""}) );
    locales.push( getLocale({IDNUMBER: 6, VERSION: ""}) );
    locales.push( getLocale({IDNUMBER: 7, TYPENUMBER: ""}) );
    break;
  }

  for(var i = 0; i < locales.length; i++) {
   let t = template;
   t = t.replace(/{TARGETLOCALE}/g, locales[i].TARGETLOCALE);
   t = t.replace(/{NAME}/, locales[i].NAME);
   t = t.replace(/{VERSION}/, locales[i].VERSION);
   t = t.replace(/{INSTALL}/, locales[i].INSTALL);
   t = t.replace(/{TYPENUMBER}/, locales[i].TYPENUMBER);
   t = t.replace(/{TYPENAME}/, locales[i].TYPENAME);
   t = t.replace(/{IDNUMBER}/g, locales[i].IDNUMBER)

   let a = appTemplate;
   a = a.replace(/{APPNAME}/, locales[i].app.APPNAME);
   a = a.replace(/{MINVERSION}/, locales[i].app.MINVERSION);
   a = a.replace(/{MAXVERSION}/, locales[i].app.MAXVERSION);
   a = a.replace(/{APPID}/, locales[i].app.APPID);

   t = t.replace(/{APPS}/, a);
   response.write(t);
  }

  response.write("</addons>");
}
