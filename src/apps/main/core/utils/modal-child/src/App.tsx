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
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
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
                className={`w-full px-4 py-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
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
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
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
                className={`w-full px-4 py-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
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
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
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
                className={`w-full px-4 py-2 text-gray-900 dark:text-white bg-white dark:bg-[#42414D] border border-gray-300 dark:border-[#42414D] rounded-md focus:outline-none focus:ring-2 focus:ring-[#0061E0] transition duration-150 ease-in-out ${
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
        <div className="mb-4 w-full">
          <label className="flex items-center cursor-pointer">
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "false")}
              render={({ field: { onChange, value } }) => (
                <input
                  type="checkbox"
                  className={`mr-2 h-4 w-4 text-[#0061E0] bg-white dark:bg-[#42414D] border-gray-300 dark:border-[#42414D] rounded focus:ring-[#0061E0] ${
                    item.classList || ""
                  }`}
                  checked={value === "true"}
                  onChange={(e) => onChange(String(e.target.checked))}
                />
              )}
            />
            <span className="text-sm text-gray-900 dark:text-white">
              {item.label}
            </span>
          </label>
        </div>
      );

    case "radio":
      return (
        <div className="mb-4 w-full">
          {item.label && (
            <label className="block text-sm font-medium mb-2 text-gray-900 dark:text-white">
              {item.label}
            </label>
          )}
          <div className="space-y-2">
            <Controller
              name={item.id}
              control={control}
              defaultValue={String(item.value || "")}
              render={({ field: { onChange, value } }) => (
                <>
                  {item.options?.map((
                    opt: { label: string; value: string },
                  ) => (
                    <label
                      key={opt.value}
                      className="flex items-center cursor-pointer mb-2"
                    >
                      <input
                        type="radio"
                        className={`mr-2 h-4 w-4 text-[#0061E0] bg-white dark:bg-[#42414D] border-gray-300 dark:border-[#42414D] focus:ring-[#0061E0] ${
                          item.classList || ""
                        }`}
                        value={opt.value}
                        checked={value === opt.value}
                        onChange={(e) => onChange(e.target.value)}
                      />
                      <span className="text-sm text-gray-900 dark:text-white">
                        {opt.label}
                      </span>
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
    console.log("Form submitted", data);
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

  globalThis.buildFormFromConfig = (config: TForm) => {
    setFormConfig(config);
  };

  return (
    <>
      <div className="min-h-screen w-full bg-white dark:bg-[#2B2A33] text-gray-900 dark:text-white flex items-center justify-center p-2">
        <FormProvider {...methods}>
          <form
            id="dynamic-form"
            className="w-full h-full flex flex-col gap-4 bg-white dark:bg-[#2B2A33] rounded-md p-4"
            onSubmit={methods.handleSubmit(onSubmit)}
          >
            {formConfig?.title && (
              <h1 className="text-xl font-bold text-gray-900 dark:text-white mb-2">
                {formConfig.title}
              </h1>
            )}
            {formConfig?.forms.map((item: TFormItem) => (
              <FormField key={item.id} item={item} control={methods.control} />
            ))}
            {formConfig && (
              <div className="flex justify-end gap-3 mt-4">
                <button
                  type="submit"
                  className="px-4 py-2 text-sm font-medium text-white bg-[#0061E0] hover:bg-[#0250BC] rounded-md shadow-sm transition duration-150 ease-in-out"
                >
                  {formConfig.submitLabel || "Submit"}
                </button>
                {formConfig.cancelLabel && (
                  <button
                    type="button"
                    className="px-4 py-2 text-sm font-medium text-gray-900 dark:text-white bg-gray-200 dark:bg-[#42414D] hover:bg-gray-300 dark:hover:bg-[#53525C] rounded-md transition duration-150 ease-in-out"
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
