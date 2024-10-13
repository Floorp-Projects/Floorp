/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type React from "react";
import { useEffect, useState } from "react";
import { Link as RouterLink, useLocation } from "react-router-dom";
import { extractTextContent } from "./extractTextContent";
import { usePageData } from "../../pageData";
import Card from "../../components/Card";
import {
  Text,
  VStack,
  Link,
  Flex,
  useColorModeValue,
  Divider,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";

interface SearchResult {
  component: React.ReactElement;
  icon: React.ReactNode;
  path: string;
  title: string;
  content: string;
}

function SearchResult({ result }: { result: SearchResult }) {
  const { t } = useTranslation();
  return (
    <Card title={result.title} icon={result.icon}>
      <Link
        as={RouterLink}
        to={result.path}
        fontSize="md"
        display="flex"
        alignItems="center"
        mb={2}
        color={useColorModeValue("blue.500", "blue.400")}
      >
        {t("search.open", { title: result.title })}
        <IconIcOutlineOpenInNew
          style={{ fontSize: "16px", marginLeft: "4px" }}
        />
      </Link>
      <Divider />
      <Text fontSize="sm" fontWeight="bold" mt={2}>
        {t("search.matchedText")}
      </Text>

      <Text fontSize="sm" mt={2}>
        {result.content}
      </Text>
    </Card>
  );
}

function SearchResults() {
  const { t } = useTranslation();
  const location = useLocation();
  const [searchResults, setSearchResults] = useState<SearchResult[]>([]);
  const pageData = usePageData();
  useEffect(() => {
    const searchParams = new URLSearchParams(location.search);
    const query = searchParams.get("q");

    if (query) {
      const resultList = Object.values(pageData).map((page) => {
        if (page.component === null) {
          return null;
        }
        const fullContent = extractTextContent(page.component);
        const matchIndex = fullContent
          ?.toLowerCase()
          .indexOf(query.toLowerCase());
        if (matchIndex !== -1 && matchIndex !== undefined) {
          const start = Math.max(0, matchIndex - 80);
          const end = Math.min((fullContent?.length ?? 0) + 80, start + 80);
          const content = `...${fullContent?.slice(start, end)}...`;
          return { ...page, content, title: page.text };
        }
        return null;
      });
      setSearchResults(resultList.filter((r) => r !== null) as SearchResult[]);
    } else {
      setSearchResults([]);
    }
  }, [location.search, pageData]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10}>
        {t("search.results")}
      </Text>

      <Text fontSize="sm" mb={8}>
        {t("search.searchResultsDescription", {
          query: new URLSearchParams(location.search).get("q") ?? "",
        })}
      </Text>

      <VStack align="stretch" spacing={6} w="100%">
        {searchResults.length > 0 ? (
          searchResults.map((result) => (
            <SearchResult key={result.path} result={result} />
          ))
        ) : (
          <Text fontSize="xl">{t("search.noResults")}</Text>
        )}
      </VStack>
    </Flex>
  );
}

export default SearchResults;
