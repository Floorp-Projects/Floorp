import { useRef } from "react";
import {
  Alert,
  AlertDescription,
  AlertDialog,
  AlertDialogBody,
  AlertDialogCloseButton,
  AlertDialogContent,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogOverlay,
  AlertIcon,
  AlertTitle,
  Button,
  Text,
} from "@chakra-ui/react";

export const RestartWarningDialog = ({
  description,
  onClose,
  isOpen,
}: {
  description: string;
  onClose: () => void;
  isOpen: boolean;
}) => {
  const cancelRef = useRef<HTMLButtonElement>(null);

  const onRestart = () => {
    window.NRRestartBrowser((result: boolean) => {
      if (result) {
        onClose();
      }
    });
  };

  return (
    <AlertDialog
      motionPreset="slideInBottom"
      leastDestructiveRef={cancelRef}
      onClose={onClose}
      isOpen={isOpen}
      isCentered
      size="lg"
    >
      <AlertDialogOverlay />

      <AlertDialogContent>
        <AlertDialogHeader>{"再起動が必要"}</AlertDialogHeader>
        <AlertDialogBody>
          <Text>{description}</Text>
          <Alert
            status="warning"
            variant="left-accent"
            roundedRight="md"
            mt={4}
          >
            <AlertDescription>
              再起動後、開かれているプライベートウインドウとプライベートタブは失われます。
            </AlertDescription>
          </Alert>
        </AlertDialogBody>
        <AlertDialogFooter>
          <Button ref={cancelRef} onClick={onClose}>
            後で
          </Button>
          <Button colorScheme="red" ml={3} onClick={onRestart}>
            今すぐ再起動
          </Button>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  );
};

export default RestartWarningDialog;
