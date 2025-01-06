/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState, useEffect, useMemo } from 'react';
import { Box, VStack, Text,  Heading, useColorModeValue } from '@chakra-ui/react';
import { usePageData } from '../../pageData';
import { extractTextContent } from './extractTextContent';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';

interface SearchResult {
  path: string;
  text: string;
  title: string;
  matches: {
    text: string;
    context: string;
  }[];
}

export default function Search() {
  const { t } = useTranslation();
  const [searchParams] = useSearchParams();
  const [results, setResults] = useState<SearchResult[]>([]);
  const navigate = useNavigate();
  const pageData = usePageData();
  const borderColor = useColorModeValue("#eee", "#2D3748");
  const hoverBgColor = useColorModeValue("gray.50", "gray.700");
  const matchBgColor = useColorModeValue("yellow.100", "yellow.700");

  const searchQuery = searchParams.get('q') || '';

  const findMatchesWithContext = (text: string, query: string): { text: string; context: string; }[] => {
    const matches: { text: string; context: string; }[] = [];
    const lowerText = text.toLowerCase();
    const lowerQuery = query.toLowerCase();
    let lastIndex = 0;

    while ((lastIndex = lowerText.indexOf(lowerQuery, lastIndex)) !== -1) {
      const matchText = text.slice(lastIndex, lastIndex + query.length);
      const contextStart = Math.max(0, lastIndex - 50);
      const contextEnd = Math.min(text.length, lastIndex + query.length + 50);
      let context = text.slice(contextStart, contextEnd);
      if (contextStart > 0) context = '...' + context;
      if (contextEnd < text.length) context += '...';

      matches.push({
        text: matchText,
        context: context
      });

      lastIndex += query.length;
    }

    return matches;
  };

  const searchIndex = useMemo(async () => {
    const index: SearchResult[] = [];

    for (const [, page] of Object.entries(pageData)) {
      if (page.component) {
        const content = await extractTextContent(page.component);
        index.push({
          path: page.path,
          text: content,
          title: page.text,
          matches: []
        });
      }
    }
    return index;
  }, []);

  useEffect(() => {
    const performSearch = async () => {
      if (!searchQuery.trim()) {
        setResults([]);
        return;
      }

      const index = await searchIndex;
      const searchResults = index
        .map(item => {
          const titleMatches = findMatchesWithContext(item.title, searchQuery);
          const contentMatches = findMatchesWithContext(item.text, searchQuery);

          return {
            ...item,
            matches: [...titleMatches, ...contentMatches]
          };
        })
        .filter(item => item.matches.length > 0);

      setResults(searchResults);
    };

    performSearch();
  }, [searchQuery, searchIndex]);

  return (
    <Box p={4} mt="80px">
      <Heading mb={4}>{t('search.title')}</Heading>
      {searchQuery && (
        <Text mb={4}>
          {t('search.resultsFor', { query: searchQuery })}
        </Text>
      )}

      <VStack spacing={4} align="stretch">
        {results.map((result, index) => (
          <Box
            key={index}
            p={4}
            borderWidth="1px"
            borderColor={borderColor}
            borderRadius="md"
            cursor="pointer"
            _hover={{ bg: hoverBgColor }}
            onClick={() => navigate(result.path)}
          >
            <Text fontWeight="bold" mb={2}>{result.title}</Text>
            <VStack align="stretch" spacing={2} mt={2}>
            <Text color="gray.500" mb={1}>{t('search.matchedText')}:</Text>
              {result.matches.map((match, matchIndex) => (
                <Box key={matchIndex} fontSize="sm" pl={2}>
                  <Text>
                    {match.context.split(new RegExp(`(${searchQuery})`, 'i')).map((part, i) =>
                      part.toLowerCase() === searchQuery.toLowerCase() ? (
                        <Box key={i} as="span" bg={matchBgColor} px={1} rounded="sm">
                          {part}
                        </Box>
                      ) : part
                    )}
                  </Text>
                </Box>
              ))}
            </VStack>
          </Box>
        ))}
        {searchQuery && results.length === 0 && (
          <Text color="gray.500">{t('search.noResults')}</Text>
        )}
      </VStack>
    </Box>
  );
}
