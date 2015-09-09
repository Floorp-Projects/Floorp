# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from symLogging import LogTrace, LogError, LogMessage

import itertools
import os
import re
import threading
import time
from bisect import bisect

# Libraries to keep prefetched
PREFETCHED_LIBS = ["xul.pdb", "firefox.pdb"]


class SymbolInfo:

    def __init__(self, addressMap):
        self.sortedAddresses = sorted(addressMap.keys())
        self.sortedSymbols = [addressMap[address]
                              for address in self.sortedAddresses]
        self.entryCount = len(self.sortedAddresses)

    # TODO: Add checks for address < funcEnd ?
    def Lookup(self, address):
        nearest = bisect(self.sortedAddresses, address) - 1
        if nearest < 0:
            return None
        return self.sortedSymbols[nearest]

    def GetEntryCount(self):
        return self.entryCount

# Singleton for .sym / .nmsym file cache management


class SymFileManager:
    # Symbol cache data structures
    sCache = {}
    sCacheCount = 0
    sCacheLock = threading.Lock()
    sMruSymbols = []

    sOptions = {}
    sCallbackTimer = None

    def __init__(self, options):
        self.sOptions = options

    def GetLibSymbolMap(self, libName, breakpadId, symbolSources):
        # Empty lib name means client couldn't associate frame with any lib
        if libName == "":
            return None

        # Check cache first
        libSymbolMap = None
        self.sCacheLock.acquire()
        try:
            if libName in self.sCache and breakpadId in self.sCache[libName]:
                libSymbolMap = self.sCache[libName][breakpadId]
                self.UpdateMruList(libName, breakpadId)
        finally:
            self.sCacheLock.release()

        if libSymbolMap is None:
            LogTrace(
                "Need to fetch PDB file for " + libName + " " + breakpadId)

            # Guess the name of the .sym or .nmsym file on disk
            if libName[-4:] == ".pdb":
                symFileNameWithoutExtension = re.sub(r"\.[^\.]+$", "", libName)
            else:
                symFileNameWithoutExtension = libName

            # Look in the symbol dirs for this .sym or .nmsym file
            for extension, source in itertools.product([".sym", ".nmsym"],
                                                       symbolSources):
                symFileName = symFileNameWithoutExtension + extension
                pathSuffix = os.sep + libName + os.sep + \
                    breakpadId + os.sep + symFileName
                path = self.sOptions["symbolPaths"][source] + pathSuffix
                libSymbolMap = self.FetchSymbolsFromFile(path)
                if libSymbolMap:
                    break

            if not libSymbolMap:
                LogTrace("No matching sym files, tried " + str(symbolSources))
                return None

            LogTrace("Storing libSymbolMap under [" + libName + "][" +
                     breakpadId + "]")
            self.sCacheLock.acquire()
            try:
                self.MaybeEvict(libSymbolMap.GetEntryCount())
                if libName not in self.sCache:
                    self.sCache[libName] = {}
                self.sCache[libName][breakpadId] = libSymbolMap
                self.sCacheCount += libSymbolMap.GetEntryCount()
                self.UpdateMruList(libName, breakpadId)
                LogTrace(str(self.sCacheCount) +
                         " symbols in cache after fetching symbol file")
            finally:
                self.sCacheLock.release()

        return libSymbolMap

    def FetchSymbolsFromFile(self, path):
        try:
            symFile = open(path, "r")
        except Exception as e:
            LogTrace("Error opening file " + path + ": " + str(e))
            return None

        LogMessage("Parsing SYM file at " + path)

        try:
            symbolMap = {}
            lineNum = 0
            publicCount = 0
            funcCount = 0
            if path.endswith(".sym"):
                for line in symFile:
                    lineNum += 1
                    if line[0:7] == "PUBLIC ":
                        line = line.rstrip()
                        fields = line.split(" ")
                        if len(fields) < 4:
                            LogTrace("Line " + str(lineNum) + " is messed")
                            continue
                        address = int(fields[1], 16)
                        symbolMap[address] = " ".join(fields[3:])
                        publicCount += 1
                    elif line[0:5] == "FUNC ":
                        line = line.rstrip()
                        fields = line.split(" ")
                        if len(fields) < 5:
                            LogTrace("Line " + str(lineNum) + " is messed")
                            continue
                        address = int(fields[1], 16)
                        symbolMap[address] = " ".join(fields[4:])
                        funcCount += 1
            elif path.endswith(".nmsym"):
                addressLength = 0
                for line in symFile:
                    lineNum += 1
                    if line.startswith(" "):
                        continue
                    if addressLength == 0:
                        addressLength = line.find(" ")
                    address = int(line[0:addressLength], 16)
                    # Some lines have the form
                    # "address space letter space symbol",
                    # some have the form "address space symbol".
                    # The letter has a meaning, but we ignore it.
                    if line[addressLength + 2] == " ":
                        symbol = line[addressLength + 3:].rstrip()
                    else:
                        symbol = line[addressLength + 1:].rstrip()
                    symbolMap[address] = symbol
                    publicCount += 1
        except Exception as e:
            LogError("Error parsing SYM file " + path)
            return None

        logString = "Found " + \
            str(len(symbolMap.keys())) + " unique entries from "
        logString += str(publicCount) + " PUBLIC lines, " + \
            str(funcCount) + " FUNC lines"
        LogTrace(logString)

        return SymbolInfo(symbolMap)

    def PrefetchRecentSymbolFiles(self):
        global PREFETCHED_LIBS

        LogMessage("Prefetching recent symbol files")
        # Schedule next timer callback
        interval = self.sOptions['prefetchInterval'] * 60 * 60
        self.sCallbackTimer = threading.Timer(
            interval, self.PrefetchRecentSymbolFiles)
        self.sCallbackTimer.start()

        thresholdTime = time.time() - \
            self.sOptions['prefetchThreshold'] * 60 * 60
        symDirsToInspect = {}
        for pdbName in PREFETCHED_LIBS:
            symDirsToInspect[pdbName] = []
            topLibPath = self.sOptions['symbolPaths'][
                'FIREFOX'] + os.sep + pdbName

            try:
                symbolDirs = os.listdir(topLibPath)
                for symbolDir in symbolDirs:
                    candidatePath = topLibPath + os.sep + symbolDir
                    mtime = os.path.getmtime(candidatePath)
                    if mtime > thresholdTime:
                        symDirsToInspect[pdbName].append(
                            (mtime, candidatePath))
            except Exception as e:
                LogError("Error while pre-fetching: " + str(e))

            LogMessage("Found " + str(len(symDirsToInspect[pdbName])) +
                       " new " + pdbName + " recent dirs")

            # Only prefetch the most recent N entries
            symDirsToInspect[pdbName].sort(reverse=True)
            symDirsToInspect[pdbName] = symDirsToInspect[pdbName][
                :self.sOptions['prefetchMaxSymbolsPerLib']]

        # Don't fetch symbols already in cache.
        # Ideally, mutex would be held from check to insert in self.sCache,
        # but we don't want to hold the lock during I/O. This won't cause
        # inconsistencies.
        self.sCacheLock.acquire()
        try:
            for pdbName in symDirsToInspect:
                for (mtime, symbolDirPath) in symDirsToInspect[pdbName]:
                    pdbId = os.path.basename(symbolDirPath)
                    if pdbName in self.sCache and \
                            pdbId in self.sCache[pdbName]:
                        symDirsToInspect[pdbName].remove(
                            (mtime, symbolDirPath))
        finally:
            self.sCacheLock.release()

        # Read all new symbol files in at once
        fetchedSymbols = {}
        fetchedCount = 0
        for pdbName in symDirsToInspect:
            # The corresponding symbol file name ends with .sym
            symFileName = re.sub(r"\.[^\.]+$", ".sym", pdbName)

            for (mtime, symbolDirPath) in symDirsToInspect[pdbName]:
                pdbId = os.path.basename(symbolDirPath)
                symbolFilePath = symbolDirPath + os.sep + symFileName
                symbolInfo = self.FetchSymbolsFromFile(symbolFilePath)
                if symbolInfo:
                    # Stop if the prefetched items are bigger than the cache
                    if fetchedCount + symbolInfo.GetEntryCount() > \
                            self.sOptions["maxCacheEntries"]:
                        break
                    fetchedSymbols[(pdbName, pdbId)] = symbolInfo
                    fetchedCount += symbolInfo.GetEntryCount()
                else:
                    LogError("Couldn't fetch .sym file symbols for " +
                             symbolFilePath)
                    continue

        # Insert new symbols into global symbol cache
        self.sCacheLock.acquire()
        try:
            # Make room for the new symbols
            self.MaybeEvict(fetchedCount)

            for (pdbName, pdbId) in fetchedSymbols:
                if pdbName not in self.sCache:
                    self.sCache[pdbName] = {}

                if pdbId in self.sCache[pdbName]:
                    continue

                newSymbolFile = fetchedSymbols[(pdbName, pdbId)]
                self.sCache[pdbName][pdbId] = newSymbolFile
                self.sCacheCount += newSymbolFile.GetEntryCount()

                # Move new symbols to front of MRU list to give them a chance
                self.UpdateMruList(pdbName, pdbId)

        finally:
            self.sCacheLock.release()

        LogMessage("Finished prefetching recent symbol files")

    def UpdateMruList(self, pdbName, pdbId):
        libId = (pdbName, pdbId)
        if libId in self.sMruSymbols:
            self.sMruSymbols.remove(libId)
        self.sMruSymbols.insert(0, libId)

    def MaybeEvict(self, freeEntriesNeeded):
        maxCacheSize = self.sOptions["maxCacheEntries"]
        LogTrace("Cache occupancy before MaybeEvict: " +
                 str(self.sCacheCount) + "/" + str(maxCacheSize))

        if self.sCacheCount == 0 or \
                self.sCacheCount + freeEntriesNeeded <= maxCacheSize:
            # No need to lock mutex here, this doesn't need to be 100%
            return

        # If adding the new entries would exceed the max cache size,
        # evict so that cache is at 70% capacity after new entries added
        numOldEntriesAfterEvict = max(
            0, (0.70 * maxCacheSize) - freeEntriesNeeded)
        numToEvict = self.sCacheCount - numOldEntriesAfterEvict

        # Evict symbols until evict quota is met, starting with least recently
        # used
        for (pdbName, pdbId) in reversed(self.sMruSymbols):
            if numToEvict <= 0:
                break

            evicteeCount = self.sCache[pdbName][pdbId].GetEntryCount()

            del self.sCache[pdbName][pdbId]
            self.sCacheCount -= evicteeCount
            self.sMruSymbols.pop()

            numToEvict -= evicteeCount

        LogTrace("Cache occupancy after MaybeEvict: " +
                 str(self.sCacheCount) + "/" + str(maxCacheSize))
