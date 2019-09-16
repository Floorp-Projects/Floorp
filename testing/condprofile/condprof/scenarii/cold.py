import asyncio


async def cold(session, options):
    # nothing is done, we just settle here for 30 seconds
    await asyncio.sleep(options.get("sleep", 30))
    return {}
