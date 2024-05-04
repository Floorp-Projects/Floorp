import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    type XULElementBase = SolidJSX.HTMLAttributes<HTMLElement> & {
      flex?: `${number}`;
    };
    interface XULBrowserElement extends XULElementBase {
      contextmenu?: string;
      message?: string;
      messagemanagergroup?: string;
      type?: "content";
      remote?: `${boolean}`;
      maychangeremoteness?: `${boolean}`;
      initiallyactive?: string;

      autocompletepopup?: string;
      src?: string;
      disablefullscreen?: `${boolean}`;
      disablehistory?: `${boolean}`;
      nodefaultsrc?: string;
      tooltip?: string;
      xmlns?: string;
      autoscroll?: `${boolean}`;
      disableglobalhistory?: `${boolean}`;
      initialBrowsingContextGroupId?: `${number}`;
      usercontextid?: `${number}`;
      changeuseragent?: `${boolean}`;
      context?: string;
    }
    interface XULMenuitemElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string;
    }
    interface IntrinsicElements {
      "xul:browser": XULBrowserElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": XULElementBase;
      "xul:linkset": XULElementBase;
      "xul:commandset": XULElementBase;
      "xul:command": XULElementBase;
      "xul:popupset": XULElementBase;
      "xul:tooltip": XULElementBase;
      "xul:panel": XULElementBase;
      "xul:menupopup": XULElementBase;
      "xul:vbox": XULElementBase;
      "xul:box": XULElementBase;
      "xul:toolbarbutton": XULElementBase;
      "xul:spacer": XULElementBase;
      "xul:splitter": XULElementBase;
      "xul:menuseparator": XULElementBase;
      "xul:menu": XULElementBase;
    }

    interface Directives {
      dndzone: Accessor<unknown[]>;
    }
  }
}
