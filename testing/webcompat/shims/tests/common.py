GPT_SHIM_MSG = "Google Publisher Tags is being shimmed by Firefox"
GAGTA_SHIM_MSG = "Google Analytics and Tag Manager is being shimmed by Firefox"


async def is_gtag_placeholder_displayed(client, url, finder, **kwargs):
    await client.navigate(url, **kwargs)
    client.execute_async_script(
        """
        const done = arguments[0];
        if (window.dataLayer?.push?.toString() === [].push.toString()) {
            return done();
        }
        setTimeout(() => {
            dataLayer.push({
                event: "datalayerReady",
                eventTimeout: 1,
                eventCallback: done,
            });
        }, 100);
    """
    )
    return client.is_displayed(client.find_element(finder))


async def clicking_link_navigates(client, url, finder, **kwargs):
    await client.navigate(url, **kwargs)
    elem = client.await_element(finder)
    return client.session.execute_async_script(
        """
        const elem = arguments[0],
              done = arguments[1];
        window.onbeforeunload = function() {
            done(true);
        };
        elem.click();
        setTimeout(() => {
            done(false);
        }, 1000);
    """,
        args=[elem],
    )


async def verify_redirectors(client, urls, expected="REDIRECTED"):
    await client.navigate(client.inline("<html>"))
    for url, type in urls.items():
        assert expected == client.execute_async_script(
            """
            const [url, type, resolve] = arguments;
            fetch(url).then(async response => {
              if (!response.ok) {
                return resolve("FAILED");
              }

              try {
                if (type === "image") {
                  const blob = await response.blob();
                  const url = URL.createObjectURL(blob);
                  const img = new Image(1, 1);
                  await new Promise((res, rej) => {
                    img.onerror = rej;
                    img.onload = res;
                    img.src = url;
                  });
                } else if (type === "js") {
                  const text = await response.text();
                  if (!text.includes("This script is intentionally empty")) {
                    throw "";
                  }
                } else {
                  return resolve("UNKNOWN TYPE");
                }
              } catch(_) {
                return resolve("TYPE MISMATCH");
              }

              resolve(response.redirected ? "REDIRECTED" : "LOADED");
            }).catch(_ => {
              resolve("BLOCKED");
            });
        """,
            url,
            type,
        )
