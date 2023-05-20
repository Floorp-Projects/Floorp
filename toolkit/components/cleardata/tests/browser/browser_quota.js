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
  relativePath.split("/").forEach(function (component) {
    file.append(component);
  });

  return file;
}

function getPath(origin) {
  // Sanitizing
  let regex = /[:\/]/g;
  return "storage/default/" + origin.replace(regex, "+");
}

// This function checks if the origin has the quota storage by checking whether
// the origin directory of that origin exists or not.
function hasQuotaStorage(origin, attr) {
  let path = getPath(origin);
  if (attr) {
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI(origin),
      attr
    );
    path += principal.originSuffix;
  }

  let file = getRelativeFile(path);
  return file.exists();
}

async function runTest(sites, deleteDataFunc) {
  info(`Adding quota storage`);
  for (let site of sites) {
    const principal = getPrincipal(site.origin, site.originAttributes);
    await addQuotaStorage(principal);
  }

  info(`Verifying ${deleteDataFunc.name}`);
  let site;
  while ((site = sites.shift())) {
    await new Promise(aResolve => {
      deleteDataFunc(...site.args, value => {
        Assert.equal(value, 0);
        aResolve();
      });
    });

    ok(
      !hasQuotaStorage(site.origin, site.originAttributes),
      `${site.origin} has no quota storage`
    );
    sites.forEach(remainSite =>
      ok(
        hasQuotaStorage(remainSite.origin, remainSite.originAttributes),
        `${remainSite.origin} has quota storage`
      )
    );
  }
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.quotaManager.testing", true],
      ["dom.simpleDB.enabled", true],
    ],
  });
});

const ORG_DOMAIN = "example.org";
const ORG_DOMAIN_SUB = `test.${ORG_DOMAIN}`;
const ORG_ORIGIN = `https://${ORG_DOMAIN}`;
const ORG_ORIGIN_SUB = `https://${ORG_DOMAIN_SUB}`;
const COM_DOMAIN = "example.com";
const COM_ORIGIN = `https://${COM_DOMAIN}`;
const LH_DOMAIN = "localhost";
const FOO_DOMAIN = "foo.com";

add_task(async function test_deleteFromHost() {
  const sites = [
    {
      args: [ORG_DOMAIN, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA],
      origin: ORG_ORIGIN,
    },
    {
      args: [COM_DOMAIN, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA],
      origin: COM_ORIGIN,
    },
    {
      args: [LH_DOMAIN, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA],
      origin: `http://${LH_DOMAIN}:8000`,
    },
    {
      args: [FOO_DOMAIN, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA],
      origin: `http://${FOO_DOMAIN}`,
      originAttributes: { userContextId: 1 },
    },
  ];

  await runTest(sites, Services.clearData.deleteDataFromHost);
});

add_task(async function test_deleteFromPrincipal() {
  const sites = [
    {
      args: [
        getPrincipal(ORG_ORIGIN),
        true,
        Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      ],
      origin: ORG_ORIGIN,
    },
    {
      args: [
        getPrincipal(COM_ORIGIN),
        true,
        Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      ],
      origin: COM_ORIGIN,
    },
  ];

  await runTest(sites, Services.clearData.deleteDataFromPrincipal);
});

add_task(async function test_deleteFromOriginAttributes() {
  const ORG_OA = { userContextId: 1 };
  const COM_OA = { userContextId: 2 };
  const sites = [
    {
      args: [ORG_OA],
      origin: ORG_ORIGIN,
      originAttributes: ORG_OA,
    },
    {
      args: [COM_OA],
      origin: COM_ORIGIN,
      originAttributes: COM_OA,
    },
  ];

  await runTest(
    sites,
    Services.clearData.deleteDataFromOriginAttributesPattern
  );
});

add_task(async function test_deleteAll() {
  info(`Adding quota storage`);
  await addQuotaStorage(getPrincipal(ORG_ORIGIN));
  await addQuotaStorage(getPrincipal(COM_ORIGIN));

  info(`Verifying deleteData`);
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

add_task(async function test_deleteSubdomain() {
  const ANOTHER_ORIGIN = `https://wwww.${ORG_DOMAIN}`;
  info(`Adding quota storage`);
  await addQuotaStorage(getPrincipal(ORG_ORIGIN));
  await addQuotaStorage(getPrincipal(ANOTHER_ORIGIN));

  info(`Verifying deleteDataFromHost for subdomain`);
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
  ok(!hasQuotaStorage(COM_ORIGIN), `${ANOTHER_ORIGIN} has no quota storage`);
});

function getOAWithPartitionKey(topLevelBaseDomain, originAttributes = {}) {
  if (!topLevelBaseDomain) {
    return originAttributes;
  }
  return {
    ...originAttributes,
    partitionKey: `(https,${topLevelBaseDomain})`,
  };
}

add_task(async function test_deleteBaseDomain() {
  info("Adding quota storage");
  await addQuotaStorage(getPrincipal(ORG_ORIGIN));
  await addQuotaStorage(getPrincipal(ORG_ORIGIN_SUB));
  await addQuotaStorage(getPrincipal(COM_ORIGIN));

  info("Adding partitioned quota storage");
  // Partitioned
  await addQuotaStorage(
    getPrincipal(COM_ORIGIN, getOAWithPartitionKey(ORG_DOMAIN))
  );
  await addQuotaStorage(
    getPrincipal(COM_ORIGIN, getOAWithPartitionKey(FOO_DOMAIN))
  );
  await addQuotaStorage(
    getPrincipal(ORG_ORIGIN, getOAWithPartitionKey(COM_DOMAIN))
  );

  info(`Verifying deleteDataFromBaseDomain`);
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
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
  ok(
    !hasQuotaStorage(ORG_ORIGIN_SUB),
    `${ORG_ORIGIN_SUB} has no quota storage`
  );
  ok(hasQuotaStorage(COM_ORIGIN), `${COM_ORIGIN} has quota storage`);

  // Partitioned
  ok(
    !hasQuotaStorage(COM_ORIGIN, getOAWithPartitionKey(ORG_DOMAIN)),
    `${COM_ORIGIN} under ${ORG_DOMAIN} has no quota storage`
  );
  ok(
    hasQuotaStorage(COM_ORIGIN, getOAWithPartitionKey(FOO_DOMAIN)),
    `${COM_ORIGIN} under ${FOO_DOMAIN} has quota storage`
  );
  ok(
    !hasQuotaStorage(ORG_ORIGIN, getOAWithPartitionKey(COM_DOMAIN)),
    `${ORG_ORIGIN} under ${COM_DOMAIN} has no quota storage`
  );

  // Cleanup
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
});
