import { useEffect, useState } from "react";

export function Clock() {
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setTime(new Date());
    }, 1000);

    return () => clearInterval(timer);
  }, []);

  const hours = time.getHours().toString().padStart(2, "0");
  const minutes = time.getMinutes().toString().padStart(2, "0");

  return (
    <div className="bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm p-3">
      <div className="flex items-center gap-3">
        <div className="text-2xl font-medium text-gray-900 dark:text-white tabular-nums tracking-wide">
          {hours}
          <span className="animate-pulse mx-0.5">:</span>
          {minutes}
        </div>
        <div className="flex flex-col text-right">
          <div className="text-sm font-medium text-gray-800 dark:text-gray-200">
            {time.toLocaleDateString(undefined, {
              weekday: "short",
            })}
          </div>
          <div className="text-xs text-gray-600 dark:text-gray-400">
            {time.toLocaleDateString(undefined, {
              month: "numeric",
              day: "numeric",
            })}
          </div>
        </div>
      </div>
    </div>
  );
}
