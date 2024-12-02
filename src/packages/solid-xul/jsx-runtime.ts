import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    type XULElementBase = SolidJSX.HTMLAttributes<HTMLElement> & {
      flex?: `${number}`;
      "data-l10n-id"?: string;
      align?: "center";
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

    interface XULMenuListElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string;
      onCommand?: () => void;
      value?: string;
    }

    interface XULMenuitemElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      type?: "checkbox";
      checked?: boolean;
      disabled?: boolean;
      oncommand?: string;
      onCommand?: () => void;
      value?: string;
    }

    interface XULRichListItem extends XULElementBase {
      value?: string;
      helpTopic?: string;
    }

    interface XULPopupSetElement extends XULElementBase {
      onpopupshowing?: string | (() => void);
    }

    interface XULMenuPopupElement extends XULElementBase {
      position?:
        | "after_start"
        | "end_before"
        | "bottomleft topleft"
        | "overlap";
      onpopupshowing?: string;
      onpopuphiding?: string;
      onPopupShowing?: (e: Event) => void;
      onPopupHiding?: (e: Event) => void;
    }

    interface XULPanelElement extends XULElementBase {
      type?: "arrow";
      position?:
        | "after_start"
        | "end_before"
        | "bottomleft topleft"
        | "overlap";
      onPopupShowing?: () => void;
      onpopupshowing?: string;
    }

    interface XULMenuElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      onpopupshowing?: string | ((event: Event) => void);
    }

    interface XULBoxElement extends XULElementBase {
      pack?: string;
      orient?: "horizontal" | "vertical";
      popup?: string;
      clicktoscroll?: boolean;
    }

    interface XULToolbarButtonElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      oncommand?: string;
      onCommand?: () => void;
      context?: string;
    }

    interface XULTabElement
      extends XULElementBase,
        HTMLAttributes<HTMLElement> {
      onwheel?: EventHandlerUnion<HTMLElement, WheelEvent>;
    }

    interface IntrinsicElements {
      "xul:arrowscrollbox": XULElementBase;
      "xul:browser": XULBrowserElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": XULElementBase;
      "xul:linkset": XULElementBase;
      "xul:popupset": XULPopupSetElement;
      "xul:tooltip": XULElementBase;
      "xul:tab": XULTabElement;
      "xul:panel": XULPanelElement;
      "xul:panelview": XULPanelElement;
      "xul:menupopup": XULMenuPopupElement;
      "xul:menulist": XULMenuListElement;
      "xul:vbox": XULBoxElement;
      "xul:hbox": XULBoxElement;
      "xul:box": XULElementBase;
      "xul:toolbar": {
        id?: string;
        toolbarname?: string;
        customizable?: string;
        mode?: string;
        context?: string;
        accesskey?: string;
        style?: string;
        class?: string;
        children: Element;
      };
      "xul:toolbarbutton": XULToolbarButtonElement;
      "xul:toolbarseparator": XULElementBase;
      "xul:spacer": XULElementBase;
      "xul:splitter": XULElementBase;
      "xul:menuseparator": XULElementBase;
      "xul:menu": XULMenuElement;
      "xul:keyset": {
        id?: string;
        children: Element;
      };
      "xul:key": {
        id?: string;
        "data-l10n-id"?: string;
        "data-l10n-attrs"?: string;
        modifiers?: string;
        keycode?: string;
        key?: string;
        command: string;
      };
      "xul:commandset": {
        id?: string;
        children: Element;
      };
      "xul:command": {
        id: string;
        oncommand: string | (() => void);
      };
      "xul:description": XULElementBase;
      "xul:checkbox": XULElementBase;
      "xul:richlistitem": XULRichListItem;
      "xul:image": XULElementBase;
      "xul:label": XULElementBase;
    }

    interface Directives {
      dndzone: Accessor<unknown[]>;
    }
  }
}
