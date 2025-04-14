import OneDriveFileList from "./OneDriveFileList.tsx";
import GmailSender from "./GmailSender";

export default function Page() {
  return (
    <div className="py-2 space-y-3">
      <OneDriveFileList />
      <GmailSender />
    </div>
  );
}
