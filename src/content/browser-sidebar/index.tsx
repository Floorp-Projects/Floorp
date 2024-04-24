import { render } from "../../components/solid-xul/solid-xul";
import { IconBar } from "./IconBar";

export class FloorpSidebar implements Hotswappable {
  new = () => {
    render(
      () => (
        <section id="SX:FloorpSidebar">
          <IconBar />
        </section>
      ),
      document.body,
    );
  };
  destroy = () => {
    //@ts-expect-error aaa
    document.querySelector("SX:FloorpSidebar").remove();
  };
}
