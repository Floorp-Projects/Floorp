import { useRef } from "react";
import { useTranslation } from "react-i18next";
import {
  Alert,
  AlertDescription,
  AlertDialog,
  AlertDialogBody,
  AlertDialogContent,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogOverlay,
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
  const { t } = useTranslation();
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
        <AlertDialogHeader>{t("restartWarningDialog.title")}</AlertDialogHeader>
        <AlertDialogBody>
          <Text>{description}</Text>
          <Alert
            status="warning"
            variant="left-accent"
            roundedRight="md"
            mt={4}
          >
            <AlertDescription>
              {t("restartWarningDialog.description")}
            </AlertDescription>
          </Alert>
        </AlertDialogBody>
        <AlertDialogFooter>
          <Button ref={cancelRef} onClick={onClose}>
            {t("restartWarningDialog.cancel")}
          </Button>
          <Button colorScheme="red" ml={3} onClick={onRestart}>
            {t("restartWarningDialog.restart")}
          </Button>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  );
};

export default RestartWarningDialog;
