/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const url =
  "about:certificate?cert=MIIGRjCCBS6gAwIBAgIQDJduPkI49CDWPd%2BG7%2Bu6kDANBgkqhkiG9w0BAQsFADBNMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMScwJQYDVQQDEx5EaWdpQ2VydCBTSEEyIFNlY3VyZSBTZXJ2ZXIgQ0EwHhcNMTgxMTA1MDAwMDAwWhcNMTkxMTEzMTIwMDAwWjCBgzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xDzANBgNVBAsTBldlYk9wczEYMBYGA1UEAxMPd3d3Lm1vemlsbGEub3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuKruymkkmkqCJh7QjmXlUOBcLFRyw5LG%2FvUUWVrsxC2gsbR8WJq%2BcYoYBpoNVStKrO4U2rBh1GEbccvT6qKOQI%2BpjjDxx9cmRdubGTGp8L0MF1ohVvhIvYLumOEoRDDPU4PvGJjGhek%2FojvedPWe8dhciHkxOC2qPFZvVFMwg1%2Fo%2Fb80147BwZQmzB18mnHsmcyKlpsCN8pxw86uao9Iun8gZQrsllW64rTZlRR56pHdAcuGAoZjYZxwS9Z%2BlvrSjEgrddemWyGGalqyFp1rXlVM1Tf4%2FIYWAQXTgTUN303u3xMjss7QK7eUDsACRxiWPLW9XQDd1c%2ByvaYJKzgJ2wIDAQABo4IC6TCCAuUwHwYDVR0jBBgwFoAUD4BhHIIxYdUvKOeNRji0LOHG2eIwHQYDVR0OBBYEFNpSvSGcN2VT%2FB9TdQ8eXwebo60%2FMCcGA1UdEQQgMB6CD3d3dy5tb3ppbGxhLm9yZ4ILbW96aWxsYS5vcmcwDgYDVR0PAQH%2FBAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjBrBgNVHR8EZDBiMC%2BgLaArhilodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc3NjYS1zaGEyLWc2LmNybDAvoC2gK4YpaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NzY2Etc2hhMi1nNi5jcmwwTAYDVR0gBEUwQzA3BglghkgBhv1sAQEwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAIBgZngQwBAgIwfAYIKwYBBQUHAQEEcDBuMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wRgYIKwYBBQUHMAKGOmh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFNIQTJTZWN1cmVTZXJ2ZXJDQS5jcnQwDAYDVR0TAQH%2FBAIwADCCAQIGCisGAQQB1nkCBAIEgfMEgfAA7gB1AKS5CZC0GFgUh7sTosxncAo8NZgE%2BRvfuON3zQ7IDdwQAAABZuYWiHwAAAQDAEYwRAIgZnMSH1JdG6NASHWTwD0mlP%2Fzbr0hzP263c02Ym0DU64CIEe4QHJDP47j0b6oTFu6RrZz1NQ9cq8Az1KnMKRuaFAlAHUAh3W%2F51l8%2BIxDmV%2B9827%2FVo1HVjb%2FSrVgwbTq%2F16ggw8AAAFm5haJAgAABAMARjBEAiAxGLXkUaOAkZhXNeNR3pWyahZeKmSaMXadgu18SfK1ZAIgKtwu5eGxK76rgaszLCZ9edBIjuU0DKorzPUuxUXFY0QwDQYJKoZIhvcNAQELBQADggEBAKLJAFO3wuaP5MM%2Fed1lhk5Uc2aDokhcM7XyvdhEKSHbgPhcgMoT9YIVoPa70gNC6KHcwoXu0g8wt7X6Vm1ql%2F68G5q844kFuC6JPl4LVT9mciD%2BVW6bHUSXD9xifL9DqdJ0Ic0SllTlM%2Boq5aAeOxUQGXhXIqj6fSQv9fQN6mXxQIoc%2Fgjxteskq%2FVl8YmY1FIZP9Bh7g27kxZ9GAAGQtjTL03RzKAuSg6yeImYVdQWasc7UPnBXlRAzZ8%2BOJThUbzK16a2CI3Rg4agKSJk%2BuA47h1%2FImmngpFLRb%2FMvRX6H1oWcUuyH6O7PZdl0YpwTpw1THIuqCGl%2FwpPgyQgcTM%3D&cert=MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8%2B8kTANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBDQTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIgU2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANyuWJBNwcQwFZA1W248ghX1LFy949v%2FcUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83nf36QYSvx6%2BM%2FhpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bdKpPDkC55gIDvEwRqFDu1m5K%2BwgdlTvza%2FP96rtxcflUxDOg5B6TXvi%2FTC2rSsd9f%2Fld0Uzs1gN2ujkSYs58O09rg1%2FRrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGXkujNVA075ME%2FOV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6%2Bguqw9ypzAO%2Bsf0%2FRR3w6RbKFfCs%2FmC%2FbdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB%2FwQIMAYBAf8CAQAwDgYDVR0PAQH%2FBAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA%2BAYRyCMWHVLyjnjUY4tCzhxtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEBCwUAA4IBAQAjPt9L0jFCpbZ%2BQlwaRMxp0Wi0XUvgBCFsS%2BJtzLHgl4%2BmUwnNqipl5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq%2FsELfeNqzqPlt%2FyGFUzZgTHbO7Djc1lGA8MXW5dRNJ2Srm8c%2BcftIl7gzbckTB%2B6WohsYFfZcTEDts8Ls%2F3HB40f%2F1LkAtDdC2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPitc%2BLJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz&cert=MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBhMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBDQTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsBCSDMAZOnTjC3U%2FdDxGkAV53ijSLdhwZAAIEJzs4bg7%2FfzTtxRuLWZscFs3YnFo97nh6Vfe63SKMI2tavegw5BmV%2FSl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt43C%2FdxC%2F%2FAH2hdmoRBBYMql1GNXRor5H4idq9Joz%2BEkIYIvUX7Q6hL%2BhqkpMfT7PT19sdl6gSzeRntwi5m3OFBqOasv%2BzbMUZBfHWymeMr%2Fy7vrTC0LUq7dBMtoM1O%2F4gdW7jVg%2FtRvoSSiicNoxBN33shbyTApOB6jtSj1etX%2BjkMOvJwIDAQABo2MwYTAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH%2FBAUwAwEB%2FzAdBgNVHQ4EFgQUA95QNVbRTLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUwDQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK%2Bt1EnE9SsPTfrgT1eXkIoyQY%2FEsrhMAtudXH%2FvTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp%2F2PV5Adg06O%2FnVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp%2BdWOIrWcBAI%2B0tKIJFPnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU%2BKrk2U886UAb3LujEV0lsYSEY1QSteDwsOoBrp%2BuvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQkCAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4%3D";

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

      for (let i = 0; i < tabs.length; i++) {
        let tabButton = certificateSection.shadowRoot.querySelector(
          `.certificate-tabs #tab${i}`
        );
        tabButton.click();

        let infoGroups = certificateSection.shadowRoot.querySelectorAll(
          "info-group"
        );
        Assert.ok(infoGroups, "infoGroups found");

        for (let infoGroup of infoGroups) {
          let infoItems = infoGroup.shadowRoot.querySelectorAll("info-item");
          Assert.ok(infoItems, "infoItems found");

          for (let infoItem of infoItems) {
            let info = infoItem.shadowRoot.children[2].textContent;
            Assert.equal(
              info.includes("undefined"),
              false,
              "Empty strings shouldn't be rendered"
            );
          }
        }
      }
    });
  });
});
