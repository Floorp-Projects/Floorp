export interface TFormItem {
  type:
    | "text"
    | "number"
    | "textarea"
    | "select"
    | "dropdown"
    | "checkbox"
    | "radio";
  id: string;
  label?: string;
  value?: string | number;
  required?: boolean;
  classList?: string;
  placeholder?: string;
  rows?: number;
  maxLength?: number;
  options?: Array<{
    label: string;
    value: string;
    icon?: string;
  }>;
}

export interface TForm {
  forms: TFormItem[];
  title: string;
  submitLabel?: string;
  cancelLabel?: string;
}

export interface TFormResult {
  success: boolean;
  error?: string;
  data: { [key: string]: string | number };
}
