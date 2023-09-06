#!/usr/bin/env python
#
# Copyright 2016 Mozilla
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# This script will fetch a thousand recent crashes from Socorro, and try to
# retrieve missing symbols from Microsoft's symbol server. It honors a list
# (ignorelist.txt) of symbols that are known to be from our applications,
# and it maintains its own list of symbols that the MS symbol server
# doesn't have (skiplist.txt).
#
# The script also depends on having write access to the directory it is
# installed in, to write the skiplist text file.

import argparse
import asyncio
import collections
import json
import logging
import os
import shutil
import zipfile
from collections import defaultdict
from tempfile import mkdtemp
from urllib.parse import quote, urljoin

from aiofile import AIOFile, LineReader
from aiohttp import ClientSession, ClientTimeout
from aiohttp.connector import TCPConnector
from aiohttp_retry import JitterRetry, RetryClient

# Just hardcoded here
MICROSOFT_SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols/"
USER_AGENT = "Microsoft-Symbol-Server/6.3.0.0"
MOZILLA_SYMBOL_SERVER = "https://symbols.mozilla.org/"
CRASHSTATS_API_URL = "https://crash-stats.mozilla.org/api/"
SUPERSEARCH_PARAM = "SuperSearch/?proto_signature=~.DLL&proto_signature=~.dll&platform=Windows&_results_number=1000"
PROCESSED_CRASHES_PARAM = "ProcessedCrash/?crash_id="
HEADERS = {"User-Agent": USER_AGENT}
SYM_SRV = "SRV*{0}*https://msdl.microsoft.com/download/symbols;SRV*{0}*https://software.intel.com/sites/downloads/symbols;SRV*{0}*https://download.amd.com/dir/bin;SRV*{0}*https://driver-symbols.nvidia.com"  # noqa
TIMEOUT = 7200
RETRIES = 5


MissingSymbol = collections.namedtuple(
    "MissingSymbol", ["debug_file", "debug_id", "filename", "code_id"]
)
log = logging.getLogger()


def get_type(data):
    # PDB v7
    if data.startswith(b"Microsoft C/C++ MSF 7.00"):
        return "pdb-v7"
    # PDB v2
    if data.startswith(b"Microsoft C/C++ program database 2.00"):
        return "pdb-v2"
    # DLL
    if data.startswith(b"MZ"):
        return "dll"
    # CAB
    if data.startswith(b"MSCF"):
        return "cab"

    return "unknown"


async def exp_backoff(retry_num):
    await asyncio.sleep(2**retry_num)


async def server_has_file(client, server, filename):
    """
    Send the symbol server a HEAD request to see if it has this symbol file.
    """
    url = urljoin(server, quote(filename))
    for i in range(RETRIES):
        try:
            async with client.head(url, headers=HEADERS, allow_redirects=True) as resp:
                if resp.status == 200 and (
                    (
                        "microsoft" in server
                        and resp.headers["Content-Type"] == "application/octet-stream"
                    )
                    or "mozilla" in server
                ):
                    log.debug(f"File exists: {url}")
                    return True
                else:
                    return False
        except Exception as e:
            # Sometimes we've SSL errors or disconnections... so in such a situation just retry
            log.warning(f"Error with {url}: retry")
            log.exception(e)
            await exp_backoff(i)

    log.debug(f"Too many retries (HEAD) for {url}: give up.")
    return False


async def fetch_file(client, server, filename):
    """
    Fetch the file from the server
    """
    url = urljoin(server, quote(filename))
    log.debug(f"Fetch url: {url}")
    for i in range(RETRIES):
        try:
            async with client.get(url, headers=HEADERS, allow_redirects=True) as resp:
                if resp.status == 200:
                    data = await resp.read()
                    typ = get_type(data)
                    if typ == "unknown":
                        # try again
                        await exp_backoff(i)
                    elif typ == "pdb-v2":
                        # too old: skip it
                        log.debug(f"PDB v2 (skipped because too old): {url}")
                        return None
                    else:
                        return data
                else:
                    log.error(f"Cannot get data (status {resp.status}) for {url}: ")
        except Exception as e:
            log.warning(f"Error with {url}")
            log.exception(e)
            await asyncio.sleep(0.5)

    log.debug(f"Too many retries (GET) for {url}: give up.")
    return None


def write_skiplist(skiplist):
    with open("skiplist.txt", "w") as sf:
        sf.writelines(
            f"{debug_id} {debug_file}\n" for debug_id, debug_file in skiplist.items()
        )


async def fetch_crash(session, url):
    async with session.get(url) as resp:
        if resp.status == 200:
            return json.loads(await resp.text())

        raise RuntimeError("Network request returned status = " + str(resp.status))


async def fetch_crashes(session, urls):
    tasks = []
    for url in urls:
        task = asyncio.create_task(fetch_crash(session, url))
        tasks.append(task)
    results = await asyncio.gather(*tasks, return_exceptions=True)
    return results


async def fetch_latest_crashes(client, url):
    async with client.get(url + SUPERSEARCH_PARAM) as resp:
        if resp.status != 200:
            resp.raise_for_status()
        data = await resp.text()
        reply = json.loads(data)
        crashes = []
        for crash in reply.get("hits"):
            if "uuid" in crash:
                crashes.append(crash.get("uuid"))
        return crashes


async def fetch_missing_symbols(url):
    log.info("Looking for missing symbols on %s" % url)
    connector = TCPConnector(limit=4, limit_per_host=0)
    missing_symbols = set()
    crash_count = 0

    client_session = ClientSession(
        headers=HEADERS, connector=connector, timeout=ClientTimeout(total=TIMEOUT)
    )
    while crash_count < 1000:
        async with RetryClient(
            client_session=client_session,
            retry_options=JitterRetry(attempts=30, statuses=[429]),
        ) as client:
            crash_uuids = await fetch_latest_crashes(client, url)
            urls = [url + PROCESSED_CRASHES_PARAM + uuid for uuid in crash_uuids]
            crashes = await fetch_crashes(client, urls)
            for crash in crashes:
                if type(crash) is not dict:
                    continue

                crash_count += 1
                modules = crash.get("json_dump").get("modules")
                for module in modules:
                    if module.get("missing_symbols"):
                        missing_symbols.add(
                            MissingSymbol(
                                module.get("debug_file"),
                                module.get("debug_id"),
                                module.get("filename"),
                                module.get("code_id"),
                            )
                        )

    return missing_symbols


async def get_list(filename):
    alist = set()
    try:
        async with AIOFile(filename, "r") as In:
            async for line in LineReader(In):
                line = line.rstrip()
                alist.add(line)
    except FileNotFoundError:
        pass

    log.debug(f"{filename} contains {len(alist)} items")

    return alist


async def get_skiplist():
    skiplist = {}
    path = "skiplist.txt"
    try:
        async with AIOFile(path, "r") as In:
            async for line in LineReader(In):
                line = line.strip()
                if line == "":
                    continue
                s = line.split(" ", maxsplit=1)
                if len(s) != 2:
                    continue
                debug_id, debug_file = s
                skiplist[debug_id] = debug_file.lower()
    except FileNotFoundError:
        pass

    log.debug(f"{path} contains {len(skiplist)} items")

    return skiplist


def get_missing_symbols(missing_symbols, skiplist, ignorelist):
    modules = defaultdict(set)
    stats = {"ignorelist": 0, "skiplist": 0}
    for symbol in missing_symbols:
        pdb = symbol.debug_file
        debug_id = symbol.debug_id
        code_file = symbol.filename
        code_id = symbol.code_id
        if pdb and debug_id and pdb.endswith(".pdb"):
            if pdb.lower() in ignorelist:
                stats["ignorelist"] += 1
                continue

            if skiplist.get(debug_id) != pdb.lower():
                modules[pdb].add((debug_id, code_file, code_id))
            else:
                stats["skiplist"] += 1
                # We've asked the symbol server previously about this,
                # so skip it.
                log.debug("%s/%s already in skiplist", pdb, debug_id)

    return modules, stats


async def collect_info(client, filename, debug_id, code_file, code_id):
    pdb_path = os.path.join(filename, debug_id, filename)
    sym_path = os.path.join(filename, debug_id, filename.replace(".pdb", "") + ".sym")

    has_pdb = await server_has_file(client, MICROSOFT_SYMBOL_SERVER, pdb_path)
    has_code = is_there = False
    if has_pdb:
        if not await server_has_file(client, MOZILLA_SYMBOL_SERVER, sym_path):
            has_code = (
                code_file
                and code_id
                and await server_has_file(
                    client,
                    MICROSOFT_SYMBOL_SERVER,
                    f"{code_file}/{code_id}/{code_file}",
                )
            )
        else:
            # if the file is on moz sym server no need to do anything
            is_there = True
            has_pdb = False

    return (filename, debug_id, code_file, code_id, has_pdb, has_code, is_there)


async def check_x86_file(path):
    async with AIOFile(path, "rb") as In:
        head = b"MODULE windows x86 "
        chunk = await In.read(len(head))
        if chunk == head:
            return True
    return False


async def run_command(cmd):
    proc = await asyncio.create_subprocess_shell(
        cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
    )
    _, err = await proc.communicate()
    err = err.decode().strip()

    return err


async def dump_module(
    output, symcache, filename, debug_id, code_file, code_id, has_code, dump_syms
):
    sym_path = os.path.join(filename, debug_id, filename.replace(".pdb", ".sym"))
    output_path = os.path.join(output, sym_path)
    sym_srv = SYM_SRV.format(symcache)
    res = {"path": sym_path, "error": "ok"}

    if has_code:
        cmd = (
            f"{dump_syms} {code_file} --code-id {code_id} --check-cfi --inlines "
            f"--store {output} --symbol-server '{sym_srv}' --verbose error"
        )
    else:
        cmd = (
            f"{dump_syms} {filename} --debug-id {debug_id} --check-cfi --inlines "
            f"--store {output} --symbol-server '{sym_srv}' --verbose error"
        )

    err = await run_command(cmd)

    if err:
        log.error(f"Error with {cmd}")
        log.error(err)
        res["error"] = "dump error"
        return res

    if not os.path.exists(output_path):
        log.error(f"Could not find file {output_path} after running {cmd}")
        res["error"] = "dump error"
        return res

    if not has_code and not await check_x86_file(output_path):
        # PDB for 32 bits contains everything we need (symbols + stack unwind info)
        # But PDB for 64 bits don't contain stack unwind info
        # (they're in the binary (.dll/.exe) itself).
        # So here we're logging because we've got a PDB (64 bits) without its DLL/EXE.
        if code_file and code_id:
            log.debug(f"x86_64 binary {code_file}/{code_id} required")
        else:
            log.debug(f"x86_64 binary for {filename}/{debug_id} required")
        res["error"] = "no binary"
        return res

    log.info(f"Successfully dumped: {filename}/{debug_id}")
    return res


async def dump(output, symcache, modules, dump_syms):
    tasks = []
    for filename, debug_id, code_file, code_id, has_code in modules:
        tasks.append(
            dump_module(
                output,
                symcache,
                filename,
                debug_id,
                code_file,
                code_id,
                has_code,
                dump_syms,
            )
        )

    res = await asyncio.gather(*tasks)

    # Even if we haven't CFI the generated file is useful to get symbols
    # from addresses so keep error == 2.
    file_index = {x["path"] for x in res if x["error"] in ["ok", "no binary"]}
    stats = {
        "dump_error": sum(1 for x in res if x["error"] == "dump error"),
        "no_bin": sum(1 for x in res if x["error"] == "no binary"),
    }

    return file_index, stats


async def collect(modules):
    loop = asyncio.get_event_loop()
    tasks = []

    # In case of errors (Too many open files), just change limit_per_host
    connector = TCPConnector(limit=100, limit_per_host=4)

    async with ClientSession(
        loop=loop, timeout=ClientTimeout(total=TIMEOUT), connector=connector
    ) as client:
        for filename, ids in modules.items():
            for debug_id, code_file, code_id in ids:
                tasks.append(
                    collect_info(client, filename, debug_id, code_file, code_id)
                )

        res = await asyncio.gather(*tasks)
    to_dump = []
    stats = {"no_pdb": 0, "is_there": 0}
    for filename, debug_id, code_file, code_id, has_pdb, has_code, is_there in res:
        if not has_pdb:
            if is_there:
                stats["is_there"] += 1
            else:
                stats["no_pdb"] += 1
                log.info(f"No pdb for {filename}/{debug_id}")
            continue

        log.info(
            f"To dump: {filename}/{debug_id}, {code_file}/{code_id} and has_code = {has_code}"
        )
        to_dump.append((filename, debug_id, code_file, code_id, has_code))

    log.info(f"Collected {len(to_dump)} files to dump")

    return to_dump, stats


async def make_dirs(path):
    loop = asyncio.get_event_loop()

    def helper(path):
        os.makedirs(path, exist_ok=True)

    await loop.run_in_executor(None, helper, path)


async def fetch_and_write(output, client, filename, file_id):
    path = os.path.join(filename, file_id, filename)
    data = await fetch_file(client, MICROSOFT_SYMBOL_SERVER, path)

    if not data:
        return False

    output_dir = os.path.join(output, filename, file_id)
    await make_dirs(output_dir)

    output_path = os.path.join(output_dir, filename)
    async with AIOFile(output_path, "wb") as Out:
        await Out.write(data)

    return True


async def fetch_all_modules(output, modules):
    loop = asyncio.get_event_loop()
    tasks = []
    fetched_modules = []

    # In case of errors (Too many open files), just change limit_per_host
    connector = TCPConnector(limit=100, limit_per_host=0)

    async with ClientSession(
        loop=loop, timeout=ClientTimeout(total=TIMEOUT), connector=connector
    ) as client:
        for filename, debug_id, code_file, code_id, has_code in modules:
            tasks.append(fetch_and_write(output, client, filename, debug_id))
            if has_code:
                tasks.append(fetch_and_write(output, client, code_file, code_id))

        res = await asyncio.gather(*tasks)
        res = iter(res)
        for filename, debug_id, code_file, code_id, has_code in modules:
            fetched_pdb = next(res)
            if has_code:
                has_code = next(res)
            if fetched_pdb:
                fetched_modules.append(
                    (filename, debug_id, code_file, code_id, has_code)
                )

    return fetched_modules


def get_base_data(url):
    async def helper(url):
        return await asyncio.gather(
            fetch_missing_symbols(url),
            # Symbols that we know belong to us, so don't ask Microsoft for them.
            get_list("ignorelist.txt"),
            # Symbols that we know belong to Microsoft, so don't skiplist them.
            get_list("known-microsoft-symbols.txt"),
            # Symbols that we've asked for in the past unsuccessfully
            get_skiplist(),
        )

    return asyncio.run(helper(url))


def gen_zip(output, output_dir, file_index):
    if not file_index:
        return

    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as z:
        for f in file_index:
            z.write(os.path.join(output_dir, f), f)
    log.info(f"Wrote zip as {output}")


def main():
    parser = argparse.ArgumentParser(
        description="Fetch missing symbols from Microsoft symbol server"
    )
    parser.add_argument(
        "--crashstats-api",
        type=str,
        help="crash-stats API URL",
        default=CRASHSTATS_API_URL,
    )
    parser.add_argument("zip", type=str, help="output zip file")
    parser.add_argument(
        "--dump-syms",
        type=str,
        help="dump_syms path",
        default=os.environ.get("DUMP_SYMS_PATH"),
    )

    args = parser.parse_args()

    assert args.dump_syms, "dump_syms path is empty"

    logging.basicConfig(level=logging.DEBUG)
    aiohttp_logger = logging.getLogger("aiohttp.client")
    aiohttp_logger.setLevel(logging.INFO)
    log.info("Started")

    missing_symbols, ignorelist, known_ms_symbols, skiplist = get_base_data(
        args.crashstats_api
    )

    modules, stats_skipped = get_missing_symbols(missing_symbols, skiplist, ignorelist)

    symbol_path = mkdtemp("symsrvfetch")
    temp_path = mkdtemp(prefix="symcache")

    modules, stats_collect = asyncio.run(collect(modules))
    modules = asyncio.run(fetch_all_modules(temp_path, modules))

    file_index, stats_dump = asyncio.run(
        dump(symbol_path, temp_path, modules, args.dump_syms)
    )

    gen_zip(args.zip, symbol_path, file_index)

    shutil.rmtree(symbol_path, True)
    shutil.rmtree(temp_path, True)

    write_skiplist(skiplist)

    if not file_index:
        log.info(f"No symbols downloaded: {len(missing_symbols)} considered")
    else:
        log.info(
            f"Total files: {len(missing_symbols)}, Stored {len(file_index)} symbol files"
        )

    log.info(
        f"{stats_collect['is_there']} already present, {stats_skipped['ignorelist']} in ignored list, "  # noqa
        f"{stats_skipped['skiplist']} skipped, {stats_collect['no_pdb']} not found, "
        f"{stats_dump['dump_error']} processed with errors, "
        f"{stats_dump['no_bin']} processed but with no binaries (x86_64)"
    )
    log.info("Finished, exiting")


if __name__ == "__main__":
    main()
