"use strict";

const server = createHttpServer({
  hosts: ["a.example.com", "b.example.com", "c.example.com"],
});
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_perf_observers_cors() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://b.example.com/"],
      content_scripts: [
        {
          matches: ["http://a.example.com/file_sample.html"],
          js: ["cs.js"],
        },
      ],
    },
    files: {
      "cs.js"() {
        let obs = new window.PerformanceObserver(list => {
          list.getEntries().forEach(e => {
            browser.test.sendMessage("observed", {
              url: e.name,
              time: e.connectEnd,
              size: e.encodedBodySize,
            });
          });
        });
        obs.observe({ entryTypes: ["resource"] });

        let b = document.createElement("link");
        b.rel = "stylesheet";

        // Simulate page including a cross-origin resource from b.example.com.
        b.wrappedJSObject.href = "http://b.example.com/file_download.txt";
        document.head.appendChild(b);

        let c = document.createElement("link");
        c.rel = "stylesheet";

        // Simulate page including a cross-origin resource from c.example.com.
        c.wrappedJSObject.href = "http://c.example.com/file_download.txt";
        document.head.appendChild(c);
      },
    },
  });

  let page = await ExtensionTestUtils.loadContentPage(
    "http://a.example.com/file_sample.html"
  );
  await extension.startup();

  let b = await extension.awaitMessage("observed");
  let c = await extension.awaitMessage("observed");

  if (b.url.startsWith("http://c.")) {
    [c, b] = [b, c];
  }

  ok(b.url.startsWith("http://b."), "Observed resource from b.example.com");
  ok(b.time > 0, "connectionEnd available from b.example.com");
  equal(b.size, 428, "encodedBodySize available from b.example.com");

  ok(c.url.startsWith("http://c."), "Observed resource from c.example.com");
  equal(c.time, 0, "connectionEnd == 0 from c.example.com");
  equal(c.size, 0, "encodedBodySize == 0 from c.example.com");

  await extension.unload();
  await page.close();
});
