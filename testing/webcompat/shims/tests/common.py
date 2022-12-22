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
