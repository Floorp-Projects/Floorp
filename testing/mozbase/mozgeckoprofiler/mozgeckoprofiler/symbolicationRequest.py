# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import re

import six
from mozlog import get_proxy_logger

LOG = get_proxy_logger("profiler")

# Precompiled regex for validating lib names
# Empty lib name means client couldn't associate frame with any lib
gLibNameRE = re.compile("[0-9a-zA-Z_+\-\.]*$")

# Maximum number of times a request can be forwarded to a different server
# for symbolication. Also prevents loops.
MAX_FORWARDED_REQUESTS = 3

if six.PY2:
    # Import for Python 2
    from urllib2 import Request, urlopen
else:
    # Import for Python 3
    from urllib.request import Request, urlopen

    # Symbolication is broken when using type 'str' in python 2.7, so we use 'basestring'.
    # But for python 3.0 compatibility, 'basestring' isn't defined, but the 'str' type works.
    # So we force 'basestring' to 'str'.
    basestring = str


class ModuleV3:
    def __init__(self, libName, breakpadId):
        self.libName = libName
        self.breakpadId = breakpadId


def getModuleV3(libName, breakpadId):
    if not isinstance(libName, basestring) or not gLibNameRE.match(libName):
        LOG.debug("Bad library name: " + str(libName))
        return None

    if not isinstance(breakpadId, basestring):
        LOG.debug("Bad breakpad id: " + str(breakpadId))
        return None

    return ModuleV3(libName, breakpadId)


class SymbolicationRequest:
    def __init__(self, symFileManager, rawRequests):
        self.Reset()
        self.symFileManager = symFileManager
        self.stacks = []
        self.combinedMemoryMap = []
        self.knownModules = []
        self.symbolSources = []
        self.ParseRequests(rawRequests)

    def Reset(self):
        self.symFileManager = None
        self.isValidRequest = False
        self.combinedMemoryMap = []
        self.knownModules = []
        self.stacks = []
        self.forwardCount = 0

    def ParseRequests(self, rawRequests):
        self.isValidRequest = False

        try:
            if not isinstance(rawRequests, dict):
                LOG.debug("Request is not a dictionary")
                return

            if "version" not in rawRequests:
                LOG.debug("Request is missing 'version' field")
                return
            version = rawRequests["version"]
            if version != 4:
                LOG.debug("Invalid version: %s" % version)
                return

            if "forwarded" in rawRequests:
                if not isinstance(rawRequests["forwarded"], (int, int)):
                    LOG.debug("Invalid 'forwards' field: %s" % rawRequests["forwarded"])
                    return
                self.forwardCount = rawRequests["forwarded"]

            # Client specifies which sets of symbols should be used
            if "symbolSources" in rawRequests:
                try:
                    sourceList = [x.upper() for x in rawRequests["symbolSources"]]
                    for source in sourceList:
                        if source in self.symFileManager.sOptions["symbolPaths"]:
                            self.symbolSources.append(source)
                        else:
                            LOG.debug("Unrecognized symbol source: " + source)
                            continue
                except Exception:
                    self.symbolSources = []
                    pass

            if not self.symbolSources:
                self.symbolSources.append(self.symFileManager.sOptions["defaultApp"])
                self.symbolSources.append(self.symFileManager.sOptions["defaultOs"])

            if "memoryMap" not in rawRequests:
                LOG.debug("Request is missing 'memoryMap' field")
                return
            memoryMap = rawRequests["memoryMap"]
            if not isinstance(memoryMap, list):
                LOG.debug("'memoryMap' field in request is not a list")

            if "stacks" not in rawRequests:
                LOG.debug("Request is missing 'stacks' field")
                return
            stacks = rawRequests["stacks"]
            if not isinstance(stacks, list):
                LOG.debug("'stacks' field in request is not a list")
                return

            # Check memory map is well-formatted
            # We try to be more permissive here with the modules. If a module is not
            # well-formatted, we ignore that one by adding a None to the clean memory map. We have
            # to add a None instead of simply omitting that module because the indexes of the
            # modules in the memory map has to match the indexes of the shared libraries in the
            # profile data.
            cleanMemoryMap = []
            for module in memoryMap:
                if not isinstance(module, list):
                    LOG.debug("Entry in memory map is not a list: " + str(module))
                    cleanMemoryMap.append(None)
                    continue

                if len(module) != 2:
                    LOG.debug(
                        "Entry in memory map is not a 2 item list: " + str(module)
                    )
                    cleanMemoryMap.append(None)
                    continue
                moduleV3 = getModuleV3(*module)

                if moduleV3 is None:
                    LOG.debug("Failed to get Module V3.")

                cleanMemoryMap.append(moduleV3)

            self.combinedMemoryMap = cleanMemoryMap
            self.knownModules = [False] * len(self.combinedMemoryMap)

            # Check stack is well-formatted
            for stack in stacks:
                if not isinstance(stack, list):
                    LOG.debug("stack is not a list")
                    return
                for entry in stack:
                    if not isinstance(entry, list):
                        LOG.debug("stack entry is not a list")
                        return
                    if len(entry) != 2:
                        LOG.debug("stack entry doesn't have exactly 2 elements")
                        return

                self.stacks.append(stack)

        except Exception as e:
            LOG.debug("Exception while parsing request: " + str(e))
            return

        self.isValidRequest = True

    def ForwardRequest(self, indexes, stack, modules, symbolicatedStack):
        LOG.debug("Forwarding " + str(len(stack)) + " PCs for symbolication")

        try:
            url = self.symFileManager.sOptions["remoteSymbolServer"]
            rawModules = []
            moduleToIndex = {}
            newIndexToOldIndex = {}
            for moduleIndex, m in modules:
                l = [m.libName, m.breakpadId]
                newModuleIndex = len(rawModules)
                rawModules.append(l)
                moduleToIndex[m] = newModuleIndex
                newIndexToOldIndex[newModuleIndex] = moduleIndex

            rawStack = []
            for entry in stack:
                moduleIndex = entry[0]
                offset = entry[1]
                module = self.combinedMemoryMap[moduleIndex]
                if module is None:
                    continue
                newIndex = moduleToIndex[module]
                rawStack.append([newIndex, offset])

            requestVersion = 4
            while True:
                requestObj = {
                    "symbolSources": self.symbolSources,
                    "stacks": [rawStack],
                    "memoryMap": rawModules,
                    "forwarded": self.forwardCount + 1,
                    "version": requestVersion,
                }
                requestJson = json.dumps(requestObj).encode()
                headers = {"Content-Type": "application/json"}
                requestHandle = Request(url, requestJson, headers)
                try:
                    response = urlopen(requestHandle)
                except Exception as e:
                    if requestVersion == 4:
                        # Try again with version 3
                        requestVersion = 3
                        continue
                    raise e
                succeededVersion = requestVersion
                break

        except Exception as e:
            LOG.error("Exception while forwarding request: " + str(e))
            return

        try:
            responseJson = json.loads(response.read())
        except Exception as e:
            LOG.error(
                "Exception while reading server response to forwarded"
                " request: " + str(e)
            )
            return

        try:
            if succeededVersion == 4:
                responseKnownModules = responseJson["knownModules"]
                for newIndex, known in enumerate(responseKnownModules):
                    if known and newIndex in newIndexToOldIndex:
                        self.knownModules[newIndexToOldIndex[newIndex]] = True

                responseSymbols = responseJson["symbolicatedStacks"][0]
            else:
                responseSymbols = responseJson[0]
            if len(responseSymbols) != len(stack):
                LOG.error(
                    str(len(responseSymbols))
                    + " symbols in response, "
                    + str(len(stack))
                    + " PCs in request!"
                )
                return

            for index in range(0, len(stack)):
                symbol = responseSymbols[index]
                originalIndex = indexes[index]
                symbolicatedStack[originalIndex] = symbol
        except Exception as e:
            LOG.error(
                "Exception while parsing server response to forwarded"
                " request: " + str(e)
            )
            return

    def Symbolicate(self, stackNum):
        # Check if we should forward requests when required sym files don't
        # exist
        shouldForwardRequests = False
        if (
            self.symFileManager.sOptions["remoteSymbolServer"]
            and self.forwardCount < MAX_FORWARDED_REQUESTS
        ):
            shouldForwardRequests = True

        # Symbolicate each PC
        pcIndex = -1
        symbolicatedStack = []
        missingSymFiles = []
        unresolvedIndexes = []
        unresolvedStack = []
        unresolvedModules = []
        stack = self.stacks[stackNum]

        for moduleIndex, module in enumerate(self.combinedMemoryMap):
            if module is None:
                continue

            if not self.symFileManager.GetLibSymbolMap(
                module.libName, module.breakpadId, self.symbolSources
            ):
                missingSymFiles.append((module.libName, module.breakpadId))
                if shouldForwardRequests:
                    unresolvedModules.append((moduleIndex, module))
            else:
                self.knownModules[moduleIndex] = True

        for entry in stack:
            pcIndex += 1
            moduleIndex = entry[0]
            offset = entry[1]
            if moduleIndex == -1:
                symbolicatedStack.append(hex(offset))
                continue
            module = self.combinedMemoryMap[moduleIndex]
            if module is None:
                continue

            if (module.libName, module.breakpadId) in missingSymFiles:
                if shouldForwardRequests:
                    unresolvedIndexes.append(pcIndex)
                    unresolvedStack.append(entry)
                symbolicatedStack.append(hex(offset) + " (in " + module.libName + ")")
                continue

            functionName = None
            libSymbolMap = self.symFileManager.GetLibSymbolMap(
                module.libName, module.breakpadId, self.symbolSources
            )
            functionName = libSymbolMap.Lookup(offset)

            if functionName is None:
                functionName = hex(offset)
            symbolicatedStack.append(functionName + " (in " + module.libName + ")")

        # Ask another server for help symbolicating unresolved addresses
        if len(unresolvedStack) > 0 or len(unresolvedModules) > 0:
            self.ForwardRequest(
                unresolvedIndexes, unresolvedStack, unresolvedModules, symbolicatedStack
            )

        return symbolicatedStack
