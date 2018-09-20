ChromeUtils.import("resource://gre/modules/Services.jsm");

/* Setting a custom permission for this website */
let uriObj = Services.io.newURI(TEST_DOMAIN);
Services.perms.add(uriObj, "cookie", Services.perms.ALLOW_ACTION);

let cookieBehavior = BEHAVIOR_REJECT_TRACKER;
let blockingByContentBlocking = true;
let blockingByContentBlockingUI = true;
let blockingByContentBlockingRTUI = false;
let blockingByAllowList = false;
let expectedBlockingNotifications = false;

let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + "/";
}
/* import-globals-from imageCacheWorker.js */
Services.scriptloader.loadSubScript(rootDir + "imageCacheWorker.js", this);
