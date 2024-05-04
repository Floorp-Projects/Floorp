import "@solid-xul/jsx-runtime";
import { createEffect, For, onMount } from "solid-js";
import { createSignal } from "solid-js";
import Sortable from "sortablejs";

const containerStyle = {
  border: "1px solid black",
  padding: "0.3em",
  "max-width": "200px",
};
const itemStyle = {
  border: "1px solid blue",
  padding: "0.3em",
  margin: "0.2em 0",
  "font-size": "20px",
};
export function IconBar() {
  const [getItems, setItems] = createSignal([
    { id: 1, title: "item 1" },
    { id: 2, title: "item 2" },
    { id: 3, title: "item 3" },
  ]);

  onMount(() => {
    Sortable.create(document.getElementById("@nora:sidebar:iconbar")!, {
      animation: 150,
      store: {
        get: (sortable) => {
          return Object.values(items).map((v) => v.id);
        },
        set: (sortable) => {
          const tmp = [];
          for (const idx of sortable.toArray()) {
            const result = items.find((v) => v.id.toString() === idx);
            if (result) tmp.push(result);
          }
          setItems(tmp);
          console.log(getItems());
        },
      },
    });
  });

  //? remove reactivity for SortableJS
  const items = getItems();

  return (
    <div style={containerStyle} id="@nora:sidebar:iconbar">
      <For each={items}>
        {(item) => (
          <div style={itemStyle} data-id={item.id}>
            {item.title}
          </div>
        )}
      </For>
    </div>
  );
}
