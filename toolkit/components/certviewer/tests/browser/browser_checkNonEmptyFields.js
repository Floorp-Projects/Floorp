/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const url =
  "about:certificate?cert=MIIIIDCCBwigAwIBAgIQdFFdOvtZKGMnFerJav62lzANBgkqhkiG9w0BAQsFADBUMQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMSUwIwYDVQQDExxHb29nbGUgSW50ZXJuZXQgQXV0aG9yaXR5IEczMB4XDTE5MDcyOTE4NDQyN1oXDTE5MTAyMTE4MjMwMFowZjELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEzARBgNVBAoMCkdvb2dsZSBMTEMxFTATBgNVBAMMDCouZ29vZ2xlLmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOOufVj7d5UF%2FN49KyXmA0Z0rlQPEicHK5CbO122uECXYbWwepNlWlqM7V1GF0vRutC8YyAlxrCsUmOTrpem7NWjggWlMIIFoTATBgNVHSUEDDAKBggrBgEFBQcDATAOBgNVHQ8BAf8EBAMCB4AwggRqBgNVHREEggRhMIIEXYIMKi5nb29nbGUuY29tgg0qLmFuZHJvaWQuY29tghYqLmFwcGVuZ2luZS5nb29nbGUuY29tghIqLmNsb3VkLmdvb2dsZS5jb22CGCouY3Jvd2Rzb3VyY2UuZ29vZ2xlLmNvbYIGKi5nLmNvgg4qLmdjcC5ndnQyLmNvbYIRKi5nY3BjZG4uZ3Z0MS5jb22CCiouZ2dwaHQuY26CFiouZ29vZ2xlLWFuYWx5dGljcy5jb22CCyouZ29vZ2xlLmNhggsqLmdvb2dsZS5jbIIOKi5nb29nbGUuY28uaW6CDiouZ29vZ2xlLmNvLmpwgg4qLmdvb2dsZS5jby51a4IPKi5nb29nbGUuY29tLmFygg8qLmdvb2dsZS5jb20uYXWCDyouZ29vZ2xlLmNvbS5icoIPKi5nb29nbGUuY29tLmNvgg8qLmdvb2dsZS5jb20ubXiCDyouZ29vZ2xlLmNvbS50coIPKi5nb29nbGUuY29tLnZuggsqLmdvb2dsZS5kZYILKi5nb29nbGUuZXOCCyouZ29vZ2xlLmZyggsqLmdvb2dsZS5odYILKi5nb29nbGUuaXSCCyouZ29vZ2xlLm5sggsqLmdvb2dsZS5wbIILKi5nb29nbGUucHSCEiouZ29vZ2xlYWRhcGlzLmNvbYIPKi5nb29nbGVhcGlzLmNughEqLmdvb2dsZWNuYXBwcy5jboIUKi5nb29nbGVjb21tZXJjZS5jb22CESouZ29vZ2xldmlkZW8uY29tggwqLmdzdGF0aWMuY26CDSouZ3N0YXRpYy5jb22CEiouZ3N0YXRpY2NuYXBwcy5jboIKKi5ndnQxLmNvbYIKKi5ndnQyLmNvbYIUKi5tZXRyaWMuZ3N0YXRpYy5jb22CDCoudXJjaGluLmNvbYIQKi51cmwuZ29vZ2xlLmNvbYIWKi55b3V0dWJlLW5vY29va2llLmNvbYINKi55b3V0dWJlLmNvbYIWKi55b3V0dWJlZWR1Y2F0aW9uLmNvbYIRKi55b3V0dWJla2lkcy5jb22CByoueXQuYmWCCyoueXRpbWcuY29tghphbmRyb2lkLmNsaWVudHMuZ29vZ2xlLmNvbYILYW5kcm9pZC5jb22CG2RldmVsb3Blci5hbmRyb2lkLmdvb2dsZS5jboIcZGV2ZWxvcGVycy5hbmRyb2lkLmdvb2dsZS5jboIEZy5jb4IIZ2dwaHQuY26CBmdvby5nbIIUZ29vZ2xlLWFuYWx5dGljcy5jb22CCmdvb2dsZS5jb22CD2dvb2dsZWNuYXBwcy5jboISZ29vZ2xlY29tbWVyY2UuY29tghhzb3VyY2UuYW5kcm9pZC5nb29nbGUuY26CCnVyY2hpbi5jb22CCnd3dy5nb28uZ2yCCHlvdXR1LmJlggt5b3V0dWJlLmNvbYIUeW91dHViZWVkdWNhdGlvbi5jb22CD3lvdXR1YmVraWRzLmNvbYIFeXQuYmUwaAYIKwYBBQUHAQEEXDBaMC0GCCsGAQUFBzAChiFodHRwOi8vcGtpLmdvb2cvZ3NyMi9HVFNHSUFHMy5jcnQwKQYIKwYBBQUHMAGGHWh0dHA6Ly9vY3NwLnBraS5nb29nL0dUU0dJQUczMB0GA1UdDgQWBBRPZJy%2FftYprTgIqQTFvmZIl%2BXaGTAMBgNVHRMBAf8EAjAAMB8GA1UdIwQYMBaAFHfCuFCaZ3Z2sS3ChtCDoH6mfrpLMCEGA1UdIAQaMBgwDAYKKwYBBAHWeQIFAzAIBgZngQwBAgIwMQYDVR0fBCowKDAmoCSgIoYgaHR0cDovL2NybC5wa2kuZ29vZy9HVFNHSUFHMy5jcmwwDQYJKoZIhvcNAQELBQADggEBADc7%2BG7Np83iC8KU6GYSKizr%2F2VbPPpBjR5EzXIdUpUgxWFQ7Faxd1LVhm22QVlmMKjjYe7JUxnaIa5Zay6pBlMr5Fq%2BHbSTuA63GOt4IW1CvB1zAOaJWfxGXaNX1Xg%2F740qWEDoe95h9Xm%2FfIcFLD86FCUITAenFKBOKdaKoBMGoDBOq7jUdn492oGBYi%2BQtXfNjvcZm0yABGTjQOtH8JzyWKx1NvNLw5gSXYH5Vmj%2FQzRBEED3%2BpIJh6vgKIGEhG%2BhCg4tx4k0nnwoHGnL4a2mcemlRIWRCcjbb%2FBoF%2B05EvQvB6PcurckBaPN8r7wWCxefYViIMnHiMmzPdwZYIA%3D&cert=MIIEXDCCA0SgAwIBAgINAeOpMBz8cgY4P5pTHTANBgkqhkiG9w0BAQsFADBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjAeFw0xNzA2MTUwMDAwNDJaFw0yMTEyMTUwMDAwNDJaMFQxCzAJBgNVBAYTAlVTMR4wHAYDVQQKExVHb29nbGUgVHJ1c3QgU2VydmljZXMxJTAjBgNVBAMTHEdvb2dsZSBJbnRlcm5ldCBBdXRob3JpdHkgRzMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDKUkvqHv%2FOJGuo2nIYaNVWXQ5IWi01CXZaz6TIHLGp%2FlOJ%2B600%2F4hbn7vn6AAB3DVzdQOts7G5pH0rJnnOFUAK71G4nzKMfHCGUksW%2Fmona%2BY2emJQ2N%2BaicwJKetPKRSIgAuPOB6Aahh8Hb2XO3h9RUk2T0HNouB2VzxoMXlkyW7XUR5mw6JkLHnA52XDVoRTWkNty5oCINLvGmnRsJ1zouAqYGVQMc%2F7sy%2B%2FEYhALrVJEA8KbtyX%2Br8snwU5C1hUrwaW6MWOARa8qBpNQcWTkaIeoYvy%2FsGIJEmjR0vFEwHdp1cSaWIr6%2F4g72n7OqXwfinu7ZYW97EfoOSQJeAzAgMBAAGjggEzMIIBLzAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB%2FwQIMAYBAf8CAQAwHQYDVR0OBBYEFHfCuFCaZ3Z2sS3ChtCDoH6mfrpLMB8GA1UdIwQYMBaAFJviB1dnHB7AagbeWbSaLd%2FcGYYuMDUGCCsGAQUFBwEBBCkwJzAlBggrBgEFBQcwAYYZaHR0cDovL29jc3AucGtpLmdvb2cvZ3NyMjAyBgNVHR8EKzApMCegJaAjhiFodHRwOi8vY3JsLnBraS5nb29nL2dzcjIvZ3NyMi5jcmwwPwYDVR0gBDgwNjA0BgZngQwBAgIwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly9wa2kuZ29vZy9yZXBvc2l0b3J5LzANBgkqhkiG9w0BAQsFAAOCAQEAHLeJluRT7bvs26gyAZ8so81trUISd7O45skDUmAge1cnxhG1P2cNmSxbWsoiCt2eux9LSD%2BPAj2LIYRFHW31%2F6xoic1k4tbWXkDCjir37xTTNqRAMPUyFRWSdvt%2BnlPqwnb8Oa2I%2FmaSJukcxDjNSfpDh%2FBd1lZNgdd%2F8cLdsE3%2BwypufJ9uXO1iQpnh9zbuFIwsIONGl1p3A8CgxkqI%2FUAih3JaGOqcpcdaCIzkBaR9uYQ1X4k2Vg5APRLouzVy7a8IVk6wuy6pm%2BT7HT4LY8ibS5FEZlfAFLSW8NwsVz9SBK2Vqn1N0PIMn5xA6NZVc7o835DLAFshEWfC7TIe3g%3D%3D&cert=MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4GA1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6%2BLm8omUVCxKs%2BIVSbC9N%2FhHD6ErPLv4dfxn%2BG07IwXNb9rfF73OX4YJYJkhD10FPe%2B3t%2Bc4isUoh7SqbKSaZeqKeMWhG8eoLrvozps6yWJQeXSpkqBy%2B0Hne%2Fig%2B1AnwblrjFuTosvNYSuetZfeLQBoZfXklqtTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzdC9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ%2FgkwpRl4pazq%2Br1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCBmTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH%2FBAUwAwEB%2FzAdBgNVHQ4EFgQUm%2BIHV2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5nbG9iYWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4GsJ0%2FWwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO291xNBrBVNpGP%2BDTKqttVCL1OmLNIG%2B6KYnX3ZHu01yiPqFbQfXf5WRDLenVOavSot%2B3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h%2Bu%2FN5GJG79G%2BdwfCMNYxdAfvDbbnvRG15RjF%2BCv6pgsH%2F76tuIMRQyV%2BdTZsXjAzlAcmgQWpzU%2FqlULRuJQ%2F7TBj0%2FVLZjmmx6BEP3ojY%2Bx1J96relc8geMJgEtslQIxq%2FH5COEBkEveegeGTLg%3D%3D";

add_task(async function test() {
  await BrowserTestUtils.withNewTab(url, async function(browser) {
    await ContentTask.spawn(browser, null, async function() {
      let certificateSection = await ContentTaskUtils.waitForCondition(() => {
        return content.document.querySelector("certificate-section");
      }, "Certificate section found");

      let tabs = certificateSection.shadowRoot.querySelector(
        ".certificate-tabs"
      ).children;

      Assert.ok(tabs, "Tabs were found");
      Assert.greater(tabs.length, 0, "There must at least one tab");

      function clickTabAndCheckSelection(tabID) {
        let selectedTabBefore = 0;

        for (let i = 0; i < tabs.length; i++) {
          if (tabs[i].className.includes("selected")) {
            selectedTabBefore = i;
            break;
          }
        }

        let tabButton = certificateSection.shadowRoot.querySelector(
          `.certificate-tabs #tab${tabID}`
        );
        tabButton.click();

        if (tabID !== 0) {
          Assert.notEqual(
            selectedTabBefore,
            tabID,
            "A new tab should be selected now"
          );
        }
      }

      for (let i = 0; i < tabs.length; i++) {
        clickTabAndCheckSelection(i);

        let infoGroups = certificateSection.shadowRoot.querySelectorAll(
          "info-group"
        );
        Assert.ok(infoGroups, "infoGroups found");

        for (let infoGroup of infoGroups) {
          let infoItems = infoGroup.shadowRoot.querySelectorAll("info-item");
          Assert.ok(infoItems, "infoItems found");

          for (let infoItem of infoItems) {
            let item = infoItem.shadowRoot.querySelector(".info");
            let info = item.textContent;
            Assert.notEqual(info, "", "Empty strings shouldn't be rendered");
          }
        }
      }
    });
  });
});
