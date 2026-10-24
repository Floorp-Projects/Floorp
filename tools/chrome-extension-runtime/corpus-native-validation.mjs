// SPDX-License-Identifier: MPL-2.0

/**
 * Pure decision boundary, also exercised by host regressions. The caller reads
 * this snapshot from the running browser and the actual installed extension.
 * @param {import("./native-test-types.ts").NativeCorpusBackgroundSnapshot} snapshot
 */
export function validateNativeCorpusBackground(snapshot) {
  if (!snapshot.strictNative) return;
  if (!snapshot.runId || !/^[a-f0-9]{32}$/.test(snapshot.runId)) {
    throw Error(
      "Strict native corpus requires its actual host invocation run ID",
    );
  }
  if (
    !snapshot.candidateIdentity ||
    snapshot.candidateIdentity.buildId !== snapshot.buildId
  ) {
    throw Error(
      "Strict native corpus requires authenticated candidate identity matching the running build",
    );
  }
  if (
    !/^158(?:\.|$)/.test(snapshot.version) || !snapshot.workerWebIDLCompiled
  ) {
    throw Error(
      "Strict native corpus requires the compiled Floorp 158 candidate",
    );
  }
  if (
    !snapshot.background?.nativeWorkerCompiled ||
    snapshot.background.serviceWorkerFallback
  ) {
    throw Error(
      "Strict native corpus rejects unavailable or fallback background conversion",
    );
  }
  if (
    (snapshot.originalServiceWorker || snapshot.convertedServiceWorker ||
      snapshot.background.selected === "service_worker") &&
    (!snapshot.originalServiceWorker ||
      snapshot.convertedServiceWorker !== snapshot.originalServiceWorker ||
      snapshot.background.selected !== "service_worker" ||
      snapshot.backgroundViewType !== "background_worker")
  ) {
    throw Error(
      "Strict native corpus requires the original service worker and actual background_worker context",
    );
  }
}
