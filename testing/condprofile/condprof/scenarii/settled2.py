import asyncio


async def settled2(session, options):
    retries = 0
    while retries < 3:
        try:
            await asyncio.wait_for(session.get("https://www.mozilla.org/en-US/"), 5)
            break
        except asyncio.TimeoutError:
            retries += 1

    # we just settle here for 5 minutes
    await asyncio.sleep(options.get("sleep", 5 * 60))
    return {}
