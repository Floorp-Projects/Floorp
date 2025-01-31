import { createBirpc } from "birpc";
import { NRSettingsParentFunctions } from "../common/defines.js";

export class NRSettingsChild extends JSWindowActorChild {
  rpcCallback: Function | null = null;
  rpc;
  constructor() {
    super();
    this.rpc = createBirpc<NRSettingsParentFunctions,{}>(
      {},
      {
        post: data => this.sendAsyncMessage("birpc",data),
        on: callback => {this.rpcCallback = callback},
        // these are required when using WebSocket
        serialize: v => JSON.stringify(v),
        deserialize: v => JSON.parse(v),
      },
    )
  }
  actorCreated() {
    console.debug("NRSettingsChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRSettingsChild 5183!");
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
      Cu.exportFunction(this.NRS_getBoolPref.bind(this), window, {
        defineAs: "NRS_getBoolPref",
      });
    }
  }
  NRSPing() {
    return true;
  }
  /**
   * Wrap a promise so content can use Promise methods.
   */
  wrapPromise<T>(promise: Promise<T>) {
    return new (this.contentWindow!.Promise as PromiseConstructorLike)<T>((resolve, reject) =>
      promise.then(resolve, reject)
    );
  }
  NRS_getBoolPref(prefName: string): PromiseLike<boolean|null> {
    return this.wrapPromise(this.rpc.getBoolPref(prefName))
  }
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "birpc":
        this.rpcCallback?.(message.data)
    }
  }
}
