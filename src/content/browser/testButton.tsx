import { render } from "@solid-xul/solid-xul";
import { createSignal } from "solid-js";

function Counter() {
  const [count, setCount] = createSignal(0);
  setInterval(() => setCount(count() + 1), 1000);
  return (
    <div
      style="font-size:30px"
      onClick={() => {
        window.alert("click!");
      }}
    >
      Count: {count()}
    </div>
  );
}

render(() => <Counter />, document.getElementById("browser"));
