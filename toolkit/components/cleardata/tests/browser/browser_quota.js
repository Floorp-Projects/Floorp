/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This function adds the quota storage by simpleDB (one of quota clients
// managed by the QuotaManager). In this function, a directory
// ${profile}/storage/default/${origin}/sdb/ and a file inside are expected to
// be added.
async function addQuotaStorage(principal) {
  let connection = Cc["@mozilla.org/dom/sdb-connection;1"].createInstance(
    Ci.nsISDBConnection
  );

  connection.init(principal);

  await new Promise((aResolve, aReject) => {
    let request = connection.open("db");
    request.callback = request => {
      if (request.resultCode == Cr.NS_OK) {
        aResolve();
      } else {
        aReject(request.resultCode);
      }
    };
  });

  await new Promise((aResolve, aReject) => {
    let request = connection.write(new ArrayBuffer(1));
    request.callback = request => {
      if (request.resultCode == Cr.NS_OK) {
        aResolve();
      } else {
        aReject(request.resultCode);
      }
    };
  });
}

function getPrincipal(url, attr = {}) {
  let uri = Services.io.newURI(url);
  let ssm = Services.scriptSecurityManager;

  return ssm.createContentPrincipal(uri, attr);
}

function getProfileDir() {
  let directoryService = Services.dirsvc;

  return directoryService.get("ProfD", Ci.nsIFile);
}

function getRelativeFile(relativePath) {
  let profileDir = getProfileDir();

  let file = profileDir.clone();
  relativePath.split("/").forEach(function(component) {
    file.append(component);
  });

  return file;
}

function getPath(origin) {
  // Santizing
  let regex = /[:\/]/g;
  return "storage/default/" + origin.replace(regex, "+");
}

// This function checks if the origin has the quota storage by checking whether
// the origin directory of that origin exists or not.
function hasQuotaStorage(origin, attr) {
  let path = getPath(origin);
  if (attr) {
    path = path + "^userContextId=" + attr.userContextId;
  }

  let file = getRelativeFile(path);
  return file.exists();
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.quotaManager.testing", true], ["dom.simpleDB.enabled", true]],
  });
});

const ORG_DOMAIN = "example.com";
const ORG_ORIGIN = `https://${ORG_DOMAIN}`;
const ORG_PRINCIPAL = getPrincipal(ORG_ORIGIN);
const COM_DOMAIN = "example.org";
const COM_ORIGIN = `https://${COM_DOMAIN}`;
const COM_PRINCIPAL = getPrincipal(COM_ORIGIN);

add_task(async function test_deleteFromHost() {
  await addQuotaStorage(ORG_PRINCIPAL);
  await addQuotaStorage(COM_PRINCIPAL);

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      ORG_DOMAIN,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(!hasQuotaStorage(ORG_ORIGIN), `${ORG_ORIGIN} has no quota storage`);
  ok(hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has quota storage`);

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      COM_DOMAIN,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(!hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has no quota storage`);
});

add_task(async function test_deleteFromPrincipal() {
  await addQuotaStorage(ORG_PRINCIPAL);
  await addQuotaStorage(COM_PRINCIPAL);

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      ORG_PRINCIPAL,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(!hasQuotaStorage(ORG_ORIGIN), `${ORG_ORIGIN} has no quota storage`);
  ok(hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has quota storage`);

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      COM_PRINCIPAL,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(!hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has no quota storage`);
});

add_task(async function test_deleteFromOriginAttributes() {
  await addQuotaStorage(getPrincipal(ORG_ORIGIN, { userContextId: 1 }));
  await addQuotaStorage(getPrincipal(COM_ORIGIN, { userContextId: 2 }));

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      { userContextId: 1 },
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(
    !hasQuotaStorage(ORG_ORIGIN, { userContextId: 1 }),
    `${ORG_ORIGIN} has no quota storage`
  );
  ok(
    hasQuotaStorage(COM_ORIGIN, { userContextId: 2 }),
    `${COM_ORIGIN} has quota storage`
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      { userContextId: 2 },
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(
    !hasQuotaStorage(COM_ORIGIN, { userContextId: 2 }),
    `${COM_ORIGIN} has no quota storage`
  );
});

add_task(async function test_deleteAll() {
  await addQuotaStorage(ORG_PRINCIPAL);
  await addQuotaStorage(COM_PRINCIPAL);

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  ok(!hasQuotaStorage(ORG_ORIGIN), `${ORG_ORIGIN} has no quota storage`);
  ok(!hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has no quota storage`);
});
