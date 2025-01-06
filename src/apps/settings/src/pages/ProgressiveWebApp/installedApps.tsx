/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useState } from "react";
import {
  Text,
  useColorModeValue,
  VStack,
  HStack,
  Avatar,
  IconButton,
  Box,
  Modal,
  ModalOverlay,
  ModalContent,
  ModalHeader,
  ModalFooter,
  ModalBody,
  ModalCloseButton,
  Button,
  Input,
  useDisclosure,
  AlertDialog,
  AlertDialogBody,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogContent,
  AlertDialogOverlay,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Card from "@/components/Card";
import { getProgressiveWebAppInstalledApps } from "./dataManager";
import type { TProgressiveWebAppObject } from "@/type";

export default function InstalledApps() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");
  const [installedApps, setInstalledApps] = useState<TProgressiveWebAppObject>({});
  const { isOpen: isRenameOpen, onOpen: onRenameOpen, onClose: onRenameClose } = useDisclosure();
  const { isOpen: isUninstallOpen, onOpen: onUninstallOpen, onClose: onUninstallClose } = useDisclosure();
  const [selectedApp, setSelectedApp] = useState<any>(null);
  const [newName, setNewName] = useState("");
  const cancelRef = React.useRef<HTMLButtonElement>(null);

  useEffect(() => {
    const fetchInstalledApps = async () => {
      const apps = await getProgressiveWebAppInstalledApps();
      setInstalledApps(apps);
    };

    fetchInstalledApps();
    document?.documentElement?.addEventListener("onfocus", fetchInstalledApps);
    return () => {
      document?.documentElement?.removeEventListener("onfocus", fetchInstalledApps);
    };
  }, []);

  const handleRename = (app: any) => {
    setSelectedApp(app);
    setNewName(app.name);
    onRenameOpen();
  };

  const handleUninstall = (app: any) => {
    setSelectedApp(app);
    onUninstallOpen();
  };

  const executeRename = () => {
    if (selectedApp && newName) {
      window.NRRenameSsb(selectedApp.id, newName);
      onRenameClose();
      fetchInstalledApps();
    }
  };

  const executeUninstall = () => {
    if (selectedApp) {
      window.NRUninstallSsb(selectedApp.id);
      onUninstallClose();
      fetchInstalledApps();
    }
  };

  const fetchInstalledApps = async () => {
    const apps = await getProgressiveWebAppInstalledApps();
    setInstalledApps(apps);
  };

  return (
    <>
      <Card
        title={t("progressiveWebApp.installedApps")}
        icon={<IconMdiAppBadgeOutline style={{ fontSize: "24px", color: "#ff7708" }} />}
      >
        <VStack spacing={4} align="stretch">
          {Object.keys(installedApps).length === 0 ? (
            <Text color={textColor}>{t("progressiveWebApp.noInstalledApps")}</Text>
          ) : (
            Object.keys(installedApps).map((app, index) => (
              <HStack key={index} spacing={4} p={2} borderRadius="md" borderWidth="1px">
                <Avatar size="sm" src={installedApps[app].icon} name={installedApps[app].name} />
                <Box flex={1}>
                  <Text fontWeight="bold">{installedApps[app].name}</Text>
                  <Text fontSize="sm" color="gray.500">{installedApps[app].start_url}</Text>
                </Box>
                <IconButton
                  aria-label={t("progressiveWebApp.renameApp")}
                  icon={<IconMdiPen style={{ fontSize: "20px" }} />}
                  size="sm"
                  variant="ghost"
                  onClick={() => handleRename(installedApps[app])}
                />
                <IconButton
                  aria-label={t("progressiveWebApp.uninstallApp")}
                  icon={<IconIcOutlineDelete style={{ fontSize: "20px" }} />}
                  size="sm"
                  variant="ghost"
                  colorScheme="red"
                  onClick={() => handleUninstall(installedApps[app])}
                />
              </HStack>
            ))
          )}
        </VStack>
      </Card>

      <Modal isOpen={isRenameOpen} onClose={onRenameClose}>
        <ModalOverlay />
        <ModalContent>
          <ModalHeader>{t("progressiveWebApp.renameApp")}</ModalHeader>
          <ModalCloseButton />
          <ModalBody>
            <Input
              value={newName}
              onChange={(e) => setNewName(e.target.value)}
              placeholder={t("progressiveWebApp.enterNewName")}
            />
          </ModalBody>
          <ModalFooter>
            <Button variant="ghost" mr={3} onClick={onRenameClose}>
              {t("progressiveWebApp.cancel")}
            </Button>
            <Button colorScheme="blue" onClick={executeRename}>
              {t("progressiveWebApp.rename")}
            </Button>
          </ModalFooter>
        </ModalContent>
      </Modal>

      <AlertDialog
        isOpen={isUninstallOpen}
        leastDestructiveRef={cancelRef as React.RefObject<HTMLButtonElement>}
        onClose={onUninstallClose}
      >
        <AlertDialogOverlay>
          <AlertDialogContent>
            <AlertDialogHeader>
              {t("progressiveWebApp.uninstallConfirmation")}
            </AlertDialogHeader>
            <AlertDialogBody>
              {t("progressiveWebApp.uninstallWarning", { name: selectedApp?.name })}
            </AlertDialogBody>
            <AlertDialogFooter>
              <Button ref={cancelRef} onClick={onUninstallClose}>
                {t("progressiveWebApp.cancel")}
              </Button>
              <Button colorScheme="red" onClick={executeUninstall} ml={3}>
                {t("progressiveWebApp.uninstall")}
              </Button>
            </AlertDialogFooter>
          </AlertDialogContent>
        </AlertDialogOverlay>
      </AlertDialog>
    </>
  );
}
