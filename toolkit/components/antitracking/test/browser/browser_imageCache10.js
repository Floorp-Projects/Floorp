ChromeUtils.import("resource://gre/modules/Services.jsm");

let cookieBehavior = BEHAVIOR_REJECT_FOREIGN;
let blockingByContentBlocking = false;
let blockingByContentBlockingUI = true;
let blockingByContentBlockingRTUI = true;
let blockingByAllowList = false;
let expectedBlockingNotifications = true;

let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + "/";
}
/* import-globals-from imageCacheWorker.js */
Services.scriptloader.loadSubScript(rootDir + "imageCacheWorker.js", this);

