/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const url =
  "about:certificate?cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO1pVzllk7ZFHzANBgkqhkiG9w0BAQsFADB1MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVkIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MDUwODAwMDAwMFoXDTIwMDYwMzEyMDAwMFowgccxHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRkwFwYLKwYBBAGCNzwCAQITCERlbGF3YXJlMRAwDgYDVQQFEwc1MTU3NTUwMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEVMBMGA1UEChMMR2l0SHViLCBJbmMuMRMwEQYDVQQDEwpnaXRodWIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxjyq8jyXDDrBTyitcnB90865tWBzpHSbindG%2FXqYQkzFMBlXmqkzC%2BFdTRBYyneZw5Pz%2BXWQvL%2B74JW6LsWNc2EF0xCEqLOJuC9zjPAqbr7uroNLghGxYf13YdqbG5oj%2F4x%2BogEG3dF%2FU5YIwVr658DKyESMV6eoYV9mDVfTuJastkqcwero%2B5ZAKfYVMLUEsMwFtoTDJFmVf6JlkOWwsxp1WcQ%2FMRQK1cyqOoUFUgYylgdh3yeCDPeF22Ax8AlQxbcaI%2BGwfQL1FB7Jy%2Bh%2BKjME9lE%2FUpgV6Qt2R1xNSmvFCBWu%2BNFX6epwFP%2FJRbkMfLz0beYFUvmMgLtwVpEPSwIDAQABo4IDeTCCA3UwHwYDVR0jBBgwFoAUPdNQpdagre7zSmAKZdMh1Pj41g8wHQYDVR0OBBYEFMnCU2FmnV%2BrJfQmzQ84mqhJ6kipMCUGA1UdEQQeMByCCmdpdGh1Yi5jb22CDnd3dy5naXRodWIuY29tMA4GA1UdDwEB%2FwQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG%2FWwCATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEBMIGIBggrBgEFBQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBSBggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0U0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgCkuQmQtBhYFIe7E6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAWNBYm0KAAAEAwBHMEUCIQDRZp38cTWsWH2GdBpe%2FuPTWnsu%2Fm4BEC2%2BdIcvSykZYgIgCP5gGv6yzaazxBK2NwGdmmyuEFNSg2pARbMJlUFgU5UAdgBWFAaaL9fC7NP14b1Esj7HRna5vJkRXMDvlJhV1onQ3QAAAWNBYm0tAAAEAwBHMEUCIQCi7omUvYLm0b2LobtEeRAYnlIo7n6JxbYdrtYdmPUWJQIgVgw1AZ51vK9ENinBg22FPxb82TvNDO05T17hxXRC2IYAdgC72d%2B8H4pxtZOUI5eqkntHOFeVCqtS6BqQlmQ2jh7RhQAAAWNBYm3fAAAEAwBHMEUCIQChzdTKUU2N%2BXcqcK0OJYrN8EYynloVxho4yPk6Dq3EPgIgdNH5u8rC3UcslQV4B9o0a0w204omDREGKTVuEpxGeOQwDQYJKoZIhvcNAQELBQADggEBAHAPWpanWOW%2Fip2oJ5grAH8mqQfaunuCVE%2Bvac%2B88lkDK%2FLVdFgl2B6kIHZiYClzKtfczG93hWvKbST4NRNHP9LiaQqdNC17e5vNHnXVUGw%2ByxyjMLGqkgepOnZ2Rb14kcTOGp4i5AuJuuaMwXmCo7jUwPwfLe1NUlVBKqg6LK0Hcq4K0sZnxE8HFxiZ92WpV2AVWjRMEc%2F2z2shNoDvxvFUYyY1Oe67xINkmyQKc%2BygSBZzyLnXSFVWmHr3u5dcaaQGGAR42v6Ydr4iL38Hd4dOiBma%2BFXsXBIqWUjbST4VXmdaol7uzFMojA4zkxQDZAvF5XgJlAFadfySna%2Fteik%3D";

/* import-globals-from ./adjustedCerts.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/certviewer/tests/browser/adjustedCerts.js",
  this
);

add_task(async function test() {
  Assert.ok(adjustedCerts, "adjustedCerts found");

  let tabName = adjustedCerts[0].tabName;
  let certItems = adjustedCerts[0].certItems;

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    await SpecialPowers.spawn(browser, [[certItems, tabName]], async function([
      adjustedCerts,
      expectedTabName,
    ]) {
      let certificateSection = await ContentTaskUtils.waitForCondition(() => {
        return content.document.querySelector("certificate-section");
      }, "Certificate section found");

      let infoGroups = certificateSection.shadowRoot.querySelectorAll(
        "info-group"
      );
      Assert.ok(infoGroups, "infoGroups found");
      Assert.equal(
        infoGroups.length,
        adjustedCerts.length,
        "infoGroups must have the same length of adjustedCerts"
      );

      let tabName = certificateSection.shadowRoot.querySelector(
        ".tab[idnumber='0']"
      ).textContent;
      Assert.equal(tabName, expectedTabName, "Tab name should be the same");

      function getElementByAttribute(source, property, target) {
        for (let elem of source) {
          if (elem.hasOwnProperty(property) && elem[property] === target) {
            return elem;
          }
        }
        return null;
      }

      function checkBooleans(got, expected) {
        info(
          "If adjustedCertElments returned true, this value should be Yes, otherwise it should be No"
        );
        let gotBool;
        if (got === "Yes") {
          gotBool = true;
        } else if (got === "No") {
          gotBool = false;
        } else {
          gotBool = null;
        }
        Assert.equal(gotBool, expected, `Expected ${expected}, got ${gotBool}`);
      }

      for (let infoGroup of infoGroups) {
        let sectionId = infoGroup.shadowRoot
          .querySelector(".info-group-title")
          .getAttribute("data-l10n-id")
          .replace("certificate-viewer-", "");

        let adjustedCertsElem = getElementByAttribute(
          adjustedCerts,
          "sectionId",
          sectionId
        );
        Assert.ok(adjustedCertsElem, "The element exists in adjustedCerts");

        let infoItems = infoGroup.shadowRoot.querySelectorAll("info-item");
        Assert.equal(
          infoItems.length,
          adjustedCertsElem.sectionItems.length,
          "sectionItems must be the same length"
        );

        let i = 0;
        // Message ID mappings
        let stringMapping = {
          signaturealgorithm: "signature-algorithm",
        };
        for (let infoItem of infoItems) {
          let infoItemLabel = infoItem.shadowRoot
            .querySelector("label")
            .getAttribute("data-l10n-id");
          let infoElem = infoItem.shadowRoot.querySelector(".info");
          let infoItemInfo = infoElem.textContent;
          let adjustedCertsElemLabel =
            adjustedCertsElem.sectionItems[i].labelId;
          let adjustedCertsElemInfo = adjustedCertsElem.sectionItems[i].info;

          if (adjustedCertsElemLabel == null) {
            adjustedCertsElemLabel = "";
          }
          adjustedCertsElemLabel = adjustedCertsElemLabel
            .replace(/\s+/g, "-")
            .replace(/\./g, "")
            .replace(/\//g, "")
            .replace(/--/g, "-")
            .toLowerCase();
          adjustedCertsElemLabel =
            stringMapping[adjustedCertsElemLabel] || adjustedCertsElemLabel;

          if (adjustedCertsElemInfo == null) {
            adjustedCertsElemInfo = "";
          }
          if (typeof adjustedCertsElemInfo === "boolean") {
            checkBooleans(infoItemInfo, adjustedCertsElemInfo);
            continue;
          }

          if (
            adjustedCertsElemLabel === "timestamp" ||
            adjustedCertsElemLabel === "not-after" ||
            adjustedCertsElemLabel === "not-before"
          ) {
            Assert.equal(
              infoElem.textContent,
              adjustedCertsElemInfo.utc,
              "Timestamps must be equal"
            );
            i++;
            continue;
          }
          if (adjustedCertsElemLabel === "download") {
            Assert.equal(infoElem.children.length, 2, "Should have 2 links.");
            for (let kid of infoElem.children) {
              Assert.equal(kid.localName, "a", "Should get a cert/chain link.");
              Assert.ok(
                kid.download.endsWith("pem"),
                "Link `download` attribute should point to pem file."
              );
              Assert.ok(
                kid.dataset.l10nId.includes("pem"),
                "Link should be labeled"
              );
              Assert.equal(
                kid.dataset.l10nId.includes("chain"),
                kid.download.includes("chain"),
                "Download label and filename should match"
              );
            }
            // Remaining tests in browser_downloadLink.js

            i++;
            continue;
          }

          if (Array.isArray(adjustedCertsElemInfo)) {
            // there is a case where we have a boolean
            adjustedCertsElemInfo = adjustedCertsElemInfo
              .toString()
              .replace(/,/g, ", ");
          }

          Assert.ok(
            infoItemLabel.includes(adjustedCertsElemLabel),
            "data-l10n-id must contain the original label"
          );

          Assert.equal(
            infoItemInfo,
            adjustedCertsElemInfo,
            "Info must be equal"
          );

          i++;
        }
      }
    });
  });
});
