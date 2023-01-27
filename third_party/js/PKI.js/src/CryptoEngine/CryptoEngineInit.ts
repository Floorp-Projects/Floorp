import * as common from "../common";
import { CryptoEngine } from "./CryptoEngine";

export function initCryptoEngine() {
  if (typeof self !== "undefined") {
    if ("crypto" in self) {
      let engineName = "webcrypto";

      // Apple Safari support
      if ("webkitSubtle" in self.crypto) {
        engineName = "safari";
      }

      common.setEngine(engineName, new CryptoEngine({ name: engineName, crypto: crypto }));
    }
  } else if (typeof crypto !== "undefined" && "webcrypto" in crypto) {
    // NodeJS ^15
    const name = "NodeJS ^15";
    const nodeCrypto = (crypto as any).webcrypto as Crypto;
    common.setEngine(name, new CryptoEngine({ name, crypto: nodeCrypto }));
  }

}
