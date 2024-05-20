import { createSignal } from "solid-js";

export const [showCSK, setShowCSK] = createSignal(false);

const onHashChange = (ev: Event | null) => {
  switch (location.hash) {
    case "#csk": {
      setShowCSK(true);
      break;
    }
    default: {
      setShowCSK(false);
    }
  }
};

export function initHashChange() {
  window.addEventListener("hashchange", onHashChange);
  onHashChange(null);
}
