/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Robson <bns_robson@hotmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

HashTable *
hashAlloc(void (*func)(unsigned char *key, void *value))
{
	HashTable	*result;

	result = calloc(sizeof(HashTable), 1);
	if (!result)
	{
		fprintf(stderr, "cannot calloc hash table\n");
		exit(0);
	}

	result->size = 130363;
	result->buckets = calloc(result->size, sizeof(HashEntry *));
	if (!result->buckets)
	{
		fprintf(stderr, "cannot calloc buckets\n");
		free(result);
		exit(0);
	}
	result->free = func;

	return result;
}

void
hashFree(HashTable *table)
{
	HashEntry	*entry;
	int		i;
	HashEntry	*next;

	for (i = 0; i < table->size; i++)
	{
		entry = table->buckets[i];
		while (entry)
		{
			if (table->free)
			{
				(*table->free)(entry->key, entry->value);
			}
			next = entry->next;
			free(entry);
			entry = next;
		}
	}
	free(table->buckets);
	free(table);
}

static unsigned long
hashValue(HashTable *table, unsigned char *key)
{
	unsigned long	g;
	unsigned long	h;
	unsigned char	*x;

	x = (unsigned char *) key;
	h = 0;
	while (*x)
	{
		h = (h << 4) + *x++;
		if ((g = h & 0xf0000000) != 0)
			h = (h ^ (g >> 24)) ^ g;
	}

	return h % table->size;
}

HashEntry *
hashLookup(HashTable *table, unsigned char *key)
{
	HashEntry	*entry;

	threadMutexLock();
	entry = table->buckets[hashValue(table, key)];
	while (entry)
	{
		if (!strcmp((char *) entry->key, (char *) key))
		{
			break;
		}
		entry = entry->next;
	}
	threadMutexUnlock();

	return entry;
}

HashEntry *
hashAdd(HashTable *table, unsigned char *key, void *value)
{
	HashEntry	*entry;
	unsigned long	i;

	threadMutexLock();
	entry = calloc(sizeof(HashEntry), 1);
	if (!entry)
	{
		fprintf(stderr, "cannot calloc hash entry\n");
		exit(0);
	}
	entry->key = key;
	entry->value = value;
	i = hashValue(table, key);
	entry->next = table->buckets[i];
	table->buckets[i] = entry;
	table->count++;
	threadMutexUnlock();

	return entry;
}

static int
compareEntries(const void *entry1, const void *entry2)
{
	return strcmp
	(
		(char *) (*((HashEntry **) entry1))->key,
		(char *) (*((HashEntry **) entry2))->key
	);
}

void
hashEnumerate(HashTable *table, void (*func)(HashEntry *))
{
	HashEntry	**array;
	HashEntry	*entry;
	int		i;
	int		j;

	array = calloc(table->count, sizeof(HashEntry *));
	if (!array)
	{
		fprintf(stderr, "cannot calloc sorting array\n");
		exit(0);
	}

	j = 0;
	for (i = 0; i < table->size; i++)
	{
		entry = table->buckets[i];
		while (entry)
		{
			array[j++] = entry;
			entry = entry->next;
		}
	}

	qsort(array, table->count, sizeof(HashEntry *), compareEntries);

	for (j = 0; j < table->count; j++)
	{
		(*func)(array[j]);
	}

	free(array);
}
