import { ClassUtilityComponent } from "@/components/tailwind-hack.tsx";
import { FormProvider, useForm } from "react-hook-form";
import { useEffect } from "react";

declare global {
  var appendChildToform: (stringElement: string) => void;
}

function App() {
  const methods = useForm();
  const onSubmit = (data: any) => {
    globalThis.dispatchEvent(
      new CustomEvent("form-submit", {
        detail: data,
      }),
    );
  };

  useEffect(() => {
    const handler = (e: Event) => {
      if (e instanceof CustomEvent) {
        const form = document.getElementById("dynamic-form");
        methods.reset(e.detail);
      }
    };

    globalThis.addEventListener("form-update", handler);
    return () => globalThis.removeEventListener("form-update", handler);
  }, [methods]);

  globalThis.appendChildToform = (stringElement: string) => {
    const form = document.getElementById("dynamic-form");
    if (form) {
      while (form.firstChild) {
        form.removeChild(form.firstChild);
      }
      const element = createElmFromStr(stringElement);
      if (element) {
        const importedElement = document.importNode(element, true);
        form.appendChild(importedElement);
      }
    }
  };

  function createElmFromStr(str: string): Element | null {
    const parser = new DOMParser();
    const doc = parser.parseFromString(str, "text/html");
    return doc.body.firstElementChild;
  }

  return (
    <>
      <ClassUtilityComponent />
      {/* Tailwind CSS Utility Component for Hack. Please do not remove it*/}
      <div className="min-h-screen w-full bg-base-100 flex items-center justify-center p-4">
        <FormProvider {...methods}>
          <form
            id="dynamic-form"
            className="w-full h-full flex flex-col items-center justify-center gap-4 max-w-4xl mx-auto"
            onSubmit={methods.handleSubmit(onSubmit)}
          >
            {/* 動的なフォーム要素がここに挿入されます */}
          </form>
        </FormProvider>
      </div>
    </>
  );
}

export default App;
