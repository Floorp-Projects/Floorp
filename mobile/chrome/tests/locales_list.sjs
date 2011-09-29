let fennecID = "{a23983c0-fd0e-11dc-95ff-0800200c9a66}";
Components.utils.import("resource://gre/modules/Services.jsm");

function getLocale(aLocaleParams, aAppParams) {
 let l = {
  TARGETLOCALE: "test",
  NAME: "Test Locale",
  VERSION: "1.0",
  INSTALL: "http://www.example.com/mylocale.xpi",
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

let gLocales = [
 getLocale({IDNUMBER: 1}),

 /* These locales should fail in the LocaleRepository */
 getLocale({IDNUMBER: 1}),                         // no duplicate ids
 getLocale({IDNUMBER: 2, INSTALL: "INVALID_URL"}),
 getLocale({IDNUMBER: 3}, {APPID: "INVALID_ID"}),
 getLocale({IDNUMBER: 3}, {MAXVERSION: "0"}),
 getLocale({IDNUMBER: 4, TARGETLOCALE: ""}),
 getLocale({IDNUMBER: 5, NAME: ""}),
 getLocale({IDNUMBER: 6, VERSION: ""}),
 getLocale({IDNUMBER: 7, TYPENUMBER: ""})
];

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
 "<guid>langpack-{TARGETLOCALE}-{IDNUMBER}@firefox-mobile.mozilla.org</guid>"+
 "<version>{VERSION}</version>"+
 "<status id=\"4\">Public</status>"+
 "<compatible_applications>{APPS}</compatible_applications>"+
 "<all_compatible_os><os>ALL</os></all_compatible_os>"+
 "<install os=\"ALL\">{INSTALL}</install><strings>\n" +
  "title=TITLE\n" +
  "continueIn=CONTINUEIN\n" +
  "name=NAME\n" +
  "choose=CHOOSE\n" +
  "chooseLanguage=CHOOSELANGUAGE\n" +
  "cancel=CANCEL\n" +
  "continue=CONTINUE\n" +
  "installing=INSTALLING\n" +
  "installerror=INSTALLERROR\n" +
  "loading=LOADING" +
 "</strings>"+
"</addon>";

function handleRequest(request, response) {

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/xml", false);

  response.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  response.write("<addons>");

  for(var i = 0; i < gLocales.length; i++) {
   let t = template;
   t = t.replace(/{TARGETLOCALE}/g, gLocales[i].TARGETLOCALE);
   t = t.replace(/{NAME}/, gLocales[i].NAME);
   t = t.replace(/{VERSION}/, gLocales[i].VERSION);
   t = t.replace(/{INSTALL}/, gLocales[i].INSTALL);
   t = t.replace(/{TYPENUMBER}/, gLocales[i].TYPENUMBER);
   t = t.replace(/{TYPENAME}/, gLocales[i].TYPENAME);
   t = t.replace(/{IDNUMBER}/, gLocales[i].IDNUMBER)

   let a = appTemplate;
   a = a.replace(/{APPNAME}/, gLocales[i].app.APPNAME);
   a = a.replace(/{MINVERSION}/, gLocales[i].app.MINVERSION);
   a = a.replace(/{MAXVERSION}/, gLocales[i].app.MAXVERSION);
   a = a.replace(/{APPID}/, gLocales[i].app.APPID);

   t = t.replace(/{APPS}/, a);
   response.write(t);
  }

  response.write("</addons>");
}
