import { render } from "solid-js/web";
import { createSignal } from "solid-js";

function Counter() {
  const [count, setCount] = createSignal(0);
  setInterval(() => setCount(count() + 1), 1000);
  return <div style="font-size:30px">Count: {count()}</div>;
}

render(() => <Counter />, document.getElementById("browser"));
