import { Info } from "lucide-react";

export function InfoTip({ description }: { description: string }) {
  return (
    <div
      className="tooltip"
      data-tip={description}
    >
      <Info size={20} />
    </div>
  );
}
