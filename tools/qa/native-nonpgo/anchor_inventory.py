# SPDX-License-Identifier: MPL-2.0
"""Fresh private group membership for an unreaped direct-child anchor.

This measures membership only. The anchor owns signal authority; a PID list does
not confer it. Resource counters and the original resource sampler are separate.
"""
import math
import os
from pathlib import Path
import subprocess
import sys
import time

from qa_core import Failure, require


BOOTSTRAP_STAGES = ('census-abi-compile', 'census-abi')
MAX_BYTES = 1024**2
VALIDATION_CODES = frozenset((
    'anchor-inventory-deadline', 'anchor-inventory-abi-missing',
    'anchor-inventory-size', 'anchor-inventory-schema', 'anchor-inventory-empty-producer',
    'inventory-protocol-size', 'inventory-protocol', 'inventory-protocol-keys',
    'inventory-protocol-queries', 'inventory-protocol-query', 'inventory-protocol-unknown',
    'inventory-protocol-failure', 'inventory-protocol-pids', 'inventory-protocol-measurement',
    'inventory-protocol-progress'))


class InventoryError(Failure):
    def __init__(self, code, details=None):
        super().__init__(code)
        self.details = details or {}


def _timeout(deadline):
    require(deadline is None or (type(deadline) in (int, float) and
            math.isfinite(deadline)), 'anchor-inventory-deadline')
    remaining = 3.0 if deadline is None else min(3.0, deadline - time.monotonic())
    require(remaining > 0, 'anchor-inventory-deadline')
    return remaining


def _ps_pids(text, pgid):
    require(type(text) is str and len(text.encode()) <= MAX_BYTES,
            'anchor-inventory-size')
    pids, seen = [], set()
    for line in text.splitlines():
        fields = line.split()
        require(len(fields) == 2 and all(item.isdecimal() for item in fields),
                'anchor-inventory-schema')
        group, pid = map(int, fields)
        require(0 <= group < 2**31 and 0 <= pid < 2**31 and pid not in seen,
                'anchor-inventory-schema')
        seen.add(pid)
        if group == pgid:
            require(pid > 0, 'anchor-inventory-schema')
            pids.append(pid)
    # A successful ps may report no members of a particular group. A missing
    # producer, failed command or malformed/partial reply cannot take this path.
    require(bool(seen), 'anchor-inventory-empty-producer')
    return sorted(pids)


def collect(pgid, *, bootstrap_stage=None, deadline=None):
    """Run one bounded producer; native failure never falls back to ps."""
    require(type(pgid) is int and 0 < pgid < 2**31, 'anchor-inventory-group')
    require(bootstrap_stage is None or (type(bootstrap_stage) is str and
            bootstrap_stage in BOOTSTRAP_STAGES),
            'anchor-inventory-bootstrap-stage')
    native = sys.platform == 'darwin' and bootstrap_stage is None
    require(sys.platform in ('darwin', 'linux'), 'anchor-inventory-platform')
    timeout = _timeout(deadline)
    backend = ('darwin-header-bound' if native else 'darwin-bootstrap-ps'
               if sys.platform == 'darwin' else 'linux-ps-test')
    details = {'operation': 'anchor-group-inventory', 'backend': backend}
    try:
        if native:
            from darwin_inventory import decode_reply
            from process_census import BINDING_KEYS
            names = (*BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI',
                     'HOME', 'TMPDIR', 'LANG', 'LC_ALL')
            env = {key: os.environ[key] for key in names if key in os.environ}
            require(all(key in env for key in (*BINDING_KEYS, 'TASK_ROOT',
                    'FLOORP_CENSUS_ABI')), 'anchor-inventory-abi-missing')
            env['PYTHONDONTWRITEBYTECODE'] = '1'
            timeout = _timeout(deadline)
            result = subprocess.run(
                [sys.executable, str(Path(__file__).with_name('darwin_inventory.py')),
                 '--pgid', str(pgid)], capture_output=True, text=True, check=True,
                timeout=timeout, env=env)
            reply = decode_reply(result.stdout, pgid)
            if reply['status'] != 'measured':
                details.update(failureCode=reply['failureCode'], phase=reply['phase'],
                               errorType=reply['errorType'])
                if 'errno' in reply:
                    details['errno'] = reply['errno']
                raise InventoryError('anchor-inventory-unavailable', details)
            pids = reply['pids']
        else:
            timeout = _timeout(deadline)
            result = subprocess.run(['/bin/ps', '-axo', 'pgid=,pid='],
                capture_output=True, text=True, check=True, timeout=timeout)
            pids = _ps_pids(result.stdout, pgid)
        # One deadline covers launch, collection and decoding. No late result
        # can renew a cleanup grace or an expired original platform budget.
        if deadline is not None:
            require(time.monotonic() < deadline, 'anchor-inventory-deadline')
        return {'status': 'complete', 'group': pgid, 'pids': pids,
                'source': backend, 'bindingVerified': True}
    except InventoryError:
        raise
    except subprocess.TimeoutExpired as error:
        details.update(errorType='TimeoutExpired', timeoutSeconds=timeout)
        if native:
            from darwin_inventory import progress_reply
            details['helperProgress'] = progress_reply(error.output)
        raise InventoryError('anchor-inventory-timeout', details) from None
    except subprocess.CalledProcessError as error:
        details.update(errorType='CalledProcessError',
                       exitCode=error.returncode if type(error.returncode) is int else None)
        raise InventoryError('anchor-inventory-producer-failed', details) from None
    except OSError as error:
        details.update(errorType='OSError', errno=error.errno if type(error.errno) is int else None)
        raise InventoryError('anchor-inventory-unavailable', details) from None
    except Failure as error:
        # Admit only fixed local validation codes, never arbitrary exception text.
        details['errorType'] = 'validation'
        code = error.args[0] if len(error.args) == 1 and type(error.args[0]) is str else None
        raise InventoryError(code if code in VALIDATION_CODES else
                             'anchor-inventory-unavailable', details) from None
    except Exception:
        details['errorType'] = 'exception'
        raise InventoryError('anchor-inventory-unavailable', details) from None
