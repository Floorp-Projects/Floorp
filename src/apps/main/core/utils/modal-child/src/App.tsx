import { ClassUtilityComponent } from "@/components/tailwind-hack.tsx";
import { Control, Controller, FormProvider, useForm } from "react-hook-form";
import { useEffect, useState } from "react";
import type {
  TForm,
  TFormItem,
} from "../../../common/modal-parent/utils/type.ts";

interface FormValues {
  [key: string]: string;
}

declare global {
  var buildFormFromConfig: (config: TForm) => void;
}

interface FormFieldProps {
  item: TFormItem;
  control: Control<FormValues>;
}

const FormField = ({ item, control }: FormFieldProps) => {
  switch (item.type) {
    case "text":
    case "number":
      return (
        <div className="form-control w-full">
          {item.label && (
            <label className="label">
              <span className="label-text">{item.label}</span>
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <input
                {...field}
                type={item.type}
                className={`input input-bordered w-full ${
                  item.classList || ""
                }`}
                placeholder={item.placeholder || ""}
                maxLength={item.maxLength}
              />
            )}
          />
        </div>
      );

    case "textarea":
      return (
        <div className="form-control w-full">
          {item.label && (
            <label className="label">
              <span className="label-text">{item.label}</span>
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <textarea
                {...field}
                className={`textarea textarea-bordered w-full ${
                  item.classList || ""
                }`}
                placeholder={item.placeholder || ""}
                rows={item.rows || 4}
              />
            )}
          />
        </div>
      );

    case "select":
    case "dropdown":
      return (
        <div className="form-control w-full">
          {item.label && (
            <label className="label">
              <span className="label-text">{item.label}</span>
            </label>
          )}
          <Controller
            name={item.id}
            control={control}
            defaultValue={String(item.value || "")}
            rules={{ required: item.required }}
            render={({ field }) => (
              <select
                {...field}
                className={`select select-bordered w-full ${
                  item.classList || ""
                }`}
              >
                {item.options?.map((opt: { label: string; value: string }) => (
                  <option key={opt.value} value={opt.value}>
                    {opt.label}
                  </option>
                ))}
              </select>
            )}
          />
        </div>
      );

    case "checkbox":
      return (
        <div className="form-control">
          <label className="label cursor-pointer">
            <span className="label-text">{item.label}</span>
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "false")}
              render={({ field: { onChange, value } }) => (
                <input
                  type="checkbox"
                  className={`checkbox ${item.classList || ""}`}
                  checked={value === "true"}
                  onChange={(e) => onChange(String(e.target.checked))}
                />
              )}
            />
          </label>
        </div>
      );

    case "radio":
      return (
        <div className="form-control">
          {item.label && (
            <label className="label">
              <span className="label-text">{item.label}</span>
            </label>
          )}
          <div className="flex gap-4">
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "")}
              render={({ field: { onChange, value } }) => (
                <>
                  {item.options?.map((
                    opt: { label: string; value: string },
                  ) => (
                    <label key={opt.value} className="label cursor-pointer">
                      <span className="label-text mr-2">{opt.label}</span>
                      <input
                        type="radio"
                        className={`radio ${item.classList || ""}`}
                        value={opt.value}
                        checked={value === opt.value}
                        onChange={(e) => onChange(e.target.value)}
                      />
                    </label>
                  ))}
                </>
              )}
            />
          </div>
        </div>
      );

    default:
      return null;
  }
};

function App() {
  const methods = useForm<FormValues>();
  const [formConfig, setFormConfig] = useState<TForm | null>(null);

  const onSubmit = (data: FormValues) => {
    globalThis.dispatchEvent(
      new CustomEvent("form-submit", {
        detail: data,
      }),
    );
  };

  useEffect(() => {
    const handler = (e: Event) => {
      if (e instanceof CustomEvent) {
        methods.reset(e.detail);
      }
    };

    globalThis.addEventListener("form-update", handler);
    return () => globalThis.removeEventListener("form-update", handler);
  }, [methods]);

  // グローバル関数の定義
  globalThis.buildFormFromConfig = (config: TForm) => {
    setFormConfig(config);
  };

  return (
    <>
      <ClassUtilityComponent />
      <div className="min-h-screen w-full bg-base-100 flex items-center justify-center p-4">
        <FormProvider {...methods}>
          <form
            id="dynamic-form"
            className="w-full h-full flex flex-col items-center justify-center gap-4 max-w-4xl mx-auto"
            onSubmit={methods.handleSubmit(onSubmit)}
          >
            {formConfig?.forms.map((item: TFormItem) => (
              <FormField key={item.id} item={item} control={methods.control} />
            ))}
            {formConfig && (
              <div className="flex gap-4 mt-4">
                <button type="submit" className="btn btn-primary">
                  {formConfig.submitLabel || "Submit"}
                </button>
                {formConfig.cancelLabel && (
                  <button
                    type="button"
                    className="btn"
                    onClick={() => self.close()}
                  >
                    {formConfig.cancelLabel}
                  </button>
                )}
              </div>
            )}
          </form>
        </FormProvider>
      </div>
    </>
  );
}

export default App;
