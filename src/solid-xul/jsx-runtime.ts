import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    interface XULBrowserElement extends SolidJSX.HTMLAttributes<HTMLElement> {
      contextmenu?: string;
      message?: string;
      messagemanagergroup?: string;
      type?: "content";
      remote?: `${boolean}`;
      maychangeremoteness?: `${boolean}`;
      initiallyactive?: string;

      autocompletepopup?: string;
      src?: string;
      flex?: string;
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
    interface XULMenuitemElement extends SolidJSX.HTMLAttributes<HTMLElement> {
      label?: string;
      accesskey?: string;
      oncommand?: string;
    }
    interface IntrinsicElements {
      "xul:browser": XULBrowserElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": SolidJSX.HTMLAttributes<HTMLElement>;
      "xul:linkset";
      "xul:commandset";
      "xul:command";
      "xul:popupset";
      "xul:tooltip";
      "xul:panel";
      "xul:menupopup";
      "xul:vbox";
      "xul:box";
      "xul:toolbarbutton";
      "xul:spacer";
      "xul:splitter";
      "xul:menuseparator";
      "xul:menu";
    }
  }
}
