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
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

#define OUTPUT_DIRECTORY "test/robot/"

typedef struct Entry
{
	int		count;
	unsigned char	*viewURL;
	unsigned char	*url;
} Entry;

typedef struct Arg
{
	int	slot;
	int	count;
	View	*view;
	URL	*url;
	char	viewFile[1024];
	int	viewFileAdded;
	char	viewURL[1024];
} Arg;

typedef struct StatusEntry
{
	time_t	time;
	char	*message;
	char	*file;
	int	line;
} StatusEntry;

typedef struct TimeEntry
{
	char	*task;
	int	count;
	double	total;
	double	min;
	double	max;
} TimeEntry;

typedef void (*Handler)(int fd);

typedef struct FD
{
	Handler	handler;
	FILE	*file;
} FD;

#define numberOfSlots 64
static Thread slots[numberOfSlots];

static Thread statusThread;
static StatusEntry statusEntries[numberOfSlots];
static int sortedStatusEntries[numberOfSlots];

static unsigned short mainPort = 40404;
static int maxFD = -1;
static FD **table = NULL;
static fd_set fdSet;

static TimeEntry times[] =
{
	{ "connect success",		0, 0.0, DBL_MAX, DBL_MIN },
	{ "connect failure",		0, 0.0, DBL_MAX, DBL_MIN },
	{ "gethostbyname_r success",	0, 0.0, DBL_MAX, DBL_MIN },
	{ "gethostbyname_r failure",	0, 0.0, DBL_MAX, DBL_MIN },
	{ "readStream",			0, 0.0, DBL_MAX, DBL_MIN },
	{ "total",			0, 0.0, DBL_MAX, DBL_MIN }
};

#if 0
static unsigned char *limitURLs[] =
{
	/*
	"http://somehost/",
	"http://somehost.somedomain.com/",
	*/
	NULL
};
#endif

static char *outFile = OUTPUT_DIRECTORY "index.html";

static char *limitDomains[16];
static int limitDomainsIndex = 0;

static int count = 0;
#define LIMIT 50
#define INTERVAL ((LIMIT / 100) ? (LIMIT / 100) : 5)

static URL *currURL = NULL;
static URL *lastURL = NULL;
static URL *urls = NULL;

static char *waiting = "waiting";

#ifdef ROBOT_LOG_ATTRIBUTES
static HashTable *attributeTable = NULL;
#endif
static HashTable *contentTypeTable = NULL;
static HashTable *httpCharsetTable = NULL;
static HashTable *httpHeaderTable = NULL;
static HashTable *metaCharsetTable = NULL;
static HashTable *schemeTable = NULL;
#ifdef ROBOT_LOG_TAGS
static HashTable *tagTable = NULL;
#endif
static HashTable *urlTable = NULL;

static HashTable *knownBadTags = NULL;

static unsigned char *badTags[] =
{
	NULL
};

static FILE *statsOut = NULL;

static char *firstURL = NULL;
static unsigned char *startTime = NULL;

void
reportStatus(void *a, char *message, char *file, int line)
{
	Arg		*arg;
	StatusEntry	*entry;

	if (!a)
	{
		return;
	}

	arg = a;
	entry = &statusEntries[arg->slot];
	time(&entry->time);
	entry->message = message;
	entry->file = file;
	entry->line = line;
}

void
reportTime(int task, struct timeval *before)
{
	struct timeval	after;
	double		span;

	gettimeofday(&after, NULL);

	threadMutexLock();
	span = (((after.tv_sec - before->tv_sec) * 1000000.0) +
		after.tv_usec - before->tv_usec);
	times[task].total += span;
	if (span < times[task].min)
	{
		times[task].min = span;
	}
	if (span > times[task].max)
	{
		times[task].max = span;
	}
	times[task].count++;
	threadMutexUnlock();
}

static void
addEntry(HashTable *table, Arg *arg, unsigned char *url, unsigned char *str)
{
	Entry		*entry;
	HashEntry	*hashEntry;

	hashEntry = hashLookup(table, str);
	if (hashEntry)
	{
		((Entry *) hashEntry->value)->count++;
	}
	else
	{
		entry = calloc(sizeof(Entry), 1);
		if (!entry)
		{
			fprintf(stderr, "cannot calloc Entry\n");
			exit(0);
		}
		entry->count = 1;
		entry->viewURL = copyString((unsigned char *) arg->viewURL);
		arg->viewFileAdded = 1;
		entry->url = copyString(url);
		hashAdd(table, copyString(str), entry);
	}
}

static void
freeEntry(unsigned char *str, void *e)
{
	free(str);
	if (e)
	{
		free(((Entry *) e)->url);
		free(((Entry *) e)->viewURL);
		free(e);
	}
}

static void
reportScheme(Arg *arg, unsigned char *url, unsigned char *scheme)
{
	addEntry(schemeTable, arg, url, scheme);
}

static void
addURLFunc(void *a, URL *url)
{
	Arg	*arg;

	if (!url->scheme)
	{
		return;
	}

	arg = a;

	reportScheme(arg, url->url, url->scheme);

	if (strcmp((char *) url->scheme, "http"))
	{
		urlFree(url);
	}
	else
	{
		threadMutexLock();
		lastURL->next = url;
		lastURL = url;
		if (!currURL)
		{
			currURL = url;
		}
		threadCondSignal();
		threadMutexUnlock();
	}
}

void
reportHTTP(void *a, Input *input)
{
	Arg	*arg;

	arg = a;

	viewHTTP(arg->view, input);
}

void
reportHTTPBody(void *a, Input *input)
{
}

void
reportHTTPHeaderName(void *a, Input *input)
{
	Arg		*arg;
	unsigned char	*name;

	arg = a;

	name = copyLower(input);
	addEntry(httpHeaderTable, arg, arg->url->url, name);
	free(name);

	viewHTTPHeaderName(arg->view, input);
}

void
reportHTTPHeaderValue(void *a, Input *input, unsigned char *url)
{
	Arg	*arg;

	arg = a;

	viewHTTPHeaderValue(arg->view, input);
}

void
reportHTML(void *a, Input *input)
{
	Arg	*arg;

	arg = a;

	viewHTML(arg->view, input);
}

void
reportHTMLText(void *a, Input *input)
{
	Arg		*arg;
	unsigned char	*p;
	unsigned char	*str;

	arg = a;

	viewHTMLText(arg->view, input);

	str = copy(input);
	p = str;
	while (*p)
	{
		if
		(
			(p[0] == '&' ) &&
			(p[1] == '#' ) &&
			(p[2] == '1' ) &&
			(p[3] == '4' ) &&
			(p[4] != '\0') &&
			(p[5] == ';' )
		)
		{
			if (p[4] == '7')
			{
				fprintf(stderr, "147: %s\n", arg->url->url);
			}
			else if (p[4] == '8')
			{
				fprintf(stderr, "148: %s\n", arg->url->url);
			}
		}
		p++;
	}
	free(str);
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
	Arg		*arg;
	HashEntry	*tagEntry;

	arg = a;
	if (html->tagIsKnown)
	{
#ifdef ROBOT_LOG_TAGS
		addEntry(tagTable, arg, arg->url->url, html->tag);
#endif
	}
	else
	{
		tagEntry = hashLookup(knownBadTags, html->tag);
		if (!tagEntry)
		{
			/* XXX
			printf("\t\"%s\",\n", html->tag);
			*/
		}
	}

	viewHTMLTag(arg->view, input);
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
	Arg	*arg;

	arg = a;
	if (html->tagIsKnown)
	{
#ifdef ROBOT_LOG_ATTRIBUTES
		addEntry(attributeTable, arg, arg->url->url,
			html->currentAttribute->name);
#endif
	}
	viewHTMLAttributeName(arg->view, input);
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
	Arg	*arg;

	arg = a;

	viewHTMLAttributeValue(arg->view, input);
}

void
reportContentType(void *a, unsigned char *contentType)
{
	Arg	*arg;

	arg = a;

	addEntry(contentTypeTable, arg, arg->url->url, contentType);
}

static void
metaHandler(void *a, HTML *html)
{
	Arg		*arg;
	HTMLAttribute	*attr;
	unsigned char	*charset;
	ContentType	*contentType;

	arg = a;

	attr = html->attributes;
	while (attr)
	{
		if
		(
			(!strcmp((char *) attr->name, "http-equiv")) &&
			(attr->value) &&
			(!strcasecmp((char *) attr->value, "content-type"))
		)
		{
			break;
		}
		attr = attr->next;
	}
	if (attr)
	{
		contentType =
			mimeParseContentType(html->currentAttribute->value);
		charset = mimeGetContentTypeParameter(contentType, "charset");
		mimeFreeContentType(contentType);
		if (charset)
		{
			addEntry(metaCharsetTable, arg, arg->url->url,
				lowerCase(charset));
			free(charset);
		}
	}
}

void
tagHandler(void *a, HTML *html)
{
}

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
	Arg	*arg;

	arg = a;

	addEntry(httpCharsetTable, arg, arg->url->url, charset);
}

static void
printEntry(HashEntry *hashEntry)
{
	Entry		*entry;
	unsigned char	*key;
	unsigned char	*url;

	entry = hashEntry->value;
	key = toHTML(hashEntry->key);
	url = toHTML(entry->url);
	fprintf
	(
		statsOut,
		"<tr><td>%s</td><td align=right>%d</td>"
			"<td><a href=%s>View Source</a></td>"
			"<td><a href=%s>%s</a></td></tr>\n",
		key,
		entry->count,
		entry->viewURL,
		url,
		url
	);
	free(key);
	free(url);
}

static void
printTable(HashTable *table, char *column1)
{
	fprintf(statsOut, "<table>\n");
	fprintf
	(
		statsOut,
		"<tr bgcolor=#cccccc><td align=center>%s</td><td>Count</td>"
			"<td align=center>View Source</td>"
			"<td align=center>Example URL</td></tr>\n",
		column1
	);
	hashEnumerate(table, printEntry);
	fprintf(statsOut, "</table>\n");
}

static void
printTimes(FILE *file)
{
	int	i;

	fprintf(file, "<table>\n");

	fprintf(file, "<tr bgcolor=#cccccc>");
	fprintf(file, "<td>Task</td>");
	fprintf(file, "<td>Count</td>");
	fprintf(file, "<td>Average</td>");
	fprintf(file, "<td>Min</td>");
	fprintf(file, "<td>Max</td>");
	fprintf(file, "</tr>");

	for (i = 0; i < REPORT_TIME_MAX; i++)
	{
		TimeEntry	*entry;

		entry = &times[i];
		fprintf(file, "<tr>");
		fprintf(file, "<td>%s</td>", entry->task);
		fprintf(file, "<td align=right>%d</td>", entry->count);
		if (entry->count)
		{
			fprintf(file, "<td align=right>%f</td>",
				(entry->total / entry->count) / 1000000);
			fprintf(file, "<td align=right>%f</td>",
				entry->min / 1000000);
			fprintf(file, "<td align=right>%f</td>",
				entry->max / 1000000);
		}
		else
		{
			fprintf(file, "<td>&nbsp;</td>");
			fprintf(file, "<td>&nbsp;</td>");
			fprintf(file, "<td>&nbsp;</td>");
		}
		fprintf(file, "</tr>");
	}

	fprintf(file, "</table>\n");
}

static void
printStats(void)
{
	char	backup[1024];
	char	**limit;
	time_t	theTime;

	sprintf(backup, "%s.bak", outFile);
	rename(outFile, backup);
	statsOut = fopen(outFile, "w");
	fprintf(statsOut, "<html><head><title>Stats</title></head><body>\n");
	fprintf(statsOut, "<table bgcolor=#cccccc>\n");
	fprintf(statsOut,
		"<tr><td>Start Time</td><td align=right>%s</td></tr>\n",
		startTime);
	time(&theTime);
	fprintf(statsOut,
		"<tr><td>Time of This File</td><td align=right>%s</td></tr>\n",
		ctime(&theTime));
	fprintf(statsOut,
		"<tr><td>Root URL</td><td align=right>%s</td></tr>\n",
		firstURL);
	fprintf(statsOut, "<tr><td>Domain Limits</td><td align=right>");
	limit = limitDomains;
	while (*limit)
	{
		fprintf(statsOut, "%s ", *limit);
		limit++;
	}
	fprintf(statsOut, "</td></tr>\n");
	fprintf(statsOut,
		"<tr><td>URLs Attempted</td><td align=right>%d</td></tr>\n",
		count);
	fprintf(statsOut,
		"<tr><td>DNS Successes</td><td align=right>%d</td></tr>\n",
		netGetDNSCount());
	fprintf(statsOut,
		"<tr><td>Connect Successes</td><td align=right>%d</td></tr>\n",
		netGetConnectCount());
	fprintf(statsOut,
		"<tr><td>Non-empty HTTP Responses</td>"
			"<td align=right>%d</td></tr>\n",
		httpGetNonEmptyHTTPResponseCount());
	fprintf(statsOut,
		"<tr><td>HTTP/1.0 or Greater</td>"
			"<td align=right>%d</td></tr>\n",
		httpGetHTTP10OrGreaterCount());
	fprintf(statsOut, "</table>\n");
	printTimes(statsOut);
	printTable(schemeTable, "URL Scheme");
	printTable(httpHeaderTable, "HTTP Header");
	printTable(contentTypeTable, "Content-Type");
	printTable(httpCharsetTable, "HTTP charset");
	printTable(metaCharsetTable, "META charset");
#ifdef ROBOT_LOG_TAGS
	printTable(tagTable, "HTML Tag");
#endif
#ifdef ROBOT_LOG_ATTRIBUTES
	printTable(attributeTable, "HTML Attribute");
#endif
	fprintf(statsOut, "</body></html>\n");
	fclose(statsOut);
}

static void
openViewFile(Arg *arg)
{
	sprintf(arg->viewURL, "%010d.html", arg->count);
	sprintf(arg->viewFile, "%s%s", OUTPUT_DIRECTORY, arg->viewURL);
	/*
	sprintf(arg->viewFile, "/dev/null");
	*/
	arg->viewFileAdded = 0;
	arg->view = viewAlloc();
	arg->view->out = fopen(arg->viewFile, "w");
	if (!arg->view->out)
	{
		fprintf(stderr, "cannot open %s for writing: %s\n",
			arg->viewFile, strerror(errno));
		exit(0);
	}
	fprintf(arg->view->out, "<html><head><title>View</title></head><body><tt>");
}

static void
closeViewFile(Arg *arg)
{
	fprintf(arg->view->out, "</tt></body></html>");
	fclose(arg->view->out);
	if (!arg->viewFileAdded)
	{
		unlink(arg->viewFile);
	}
	arg->viewFileAdded = 0;
	FREE(arg->view);
}

static void
processURL(Arg *arg)
{
	struct timeval	theTime;

	gettimeofday(&theTime, NULL);

	reportStatus(arg, "processURL", __FILE__, __LINE__);

	openViewFile(arg);
	httpFree(httpProcess(arg, arg->url, NULL));
	closeViewFile(arg);

	reportStatus(arg, "processURL done", __FILE__, __LINE__);

	reportTime(REPORT_TIME_TOTAL, &theTime);
}

static void *
startHere(void *a)
{
	Arg	arg;

	arg.slot = (int) a;
	while (1)
	{
		threadMutexLock();
		while ((!currURL) && (count < LIMIT))
		{
			reportStatus(&arg, waiting, __FILE__, __LINE__);
			threadCondWait();
		}
		if (count >= LIMIT)
		{
			threadMutexUnlock();
			break;
		}
		count++;
		if (!(count % INTERVAL))
		{
			printStats();
		}
		arg.count = count;
		arg.url = currURL;
		currURL = currURL->next;
		threadMutexUnlock();
		processURL(&arg);
	}

	return NULL;
}

static int
allThreadsAreWaiting(void)
{
	int	i;

	for (i = 0; i < numberOfSlots; i++)
	{
		if (statusEntries[i].message != waiting)
		{
			break;
		}
	}

	return i == numberOfSlots;
}

static void
initKnownBadTags(void)
{
	unsigned char	**p;

	knownBadTags = hashAlloc(NULL);
	p = badTags;
	while (*p)
	{
		hashAdd(knownBadTags, *p, NULL);
		p++;
	}
}

static FD *
addFD(int fd, Handler func)
{
	FD	*f;

	if (fd > maxFD)
	{
		if (table)
		{
			table = utilRealloc(table,
				(maxFD + 1) * sizeof(*table),
				(fd + 1) * sizeof(*table));
		}
		else
		{
			table = calloc(fd + 1, sizeof(*table));
		}
		if (!table)
		{
			return NULL;
		}
		maxFD = fd;
	}

	f = malloc(sizeof(FD));
	if (!f)
	{
		return NULL;
	}
	f->handler = func;
	f->file = NULL;
	/*
	f->id = -1;
	f->port = 0;
	f->suspend = 0;
	f->writeFD = -1;
	*/

	table[fd] = f;

	FD_SET(fd, &fdSet);

	return f;
}

static void
removeFD(int fd)
{
	FD	*f;

	f = table[fd];
	if (f)
	{
		FD_CLR(fd, &fdSet);
		if (f->file && (fileno(f->file) == fd))
		{
			fclose(f->file);
		}
		else
		{
			close(fd);
		}
		free(f);
		table[fd] = NULL;
	}
}

static int
compareStatusEntries(const void *e1, const void *e2)
{
	StatusEntry	*entry1;
	StatusEntry	*entry2;

	entry1 = &statusEntries[*((int *) e1)];
	entry2 = &statusEntries[*((int *) e2)];

	return entry1->time - entry2->time;
}

static void
readClientRequest(int fd)
{
	unsigned char	buf[10240];
	int		bytesRead;
	FILE		*file;
	int		i;

	bytesRead = recv(fd, buf, sizeof(buf) - 1, 0);
	if (bytesRead < 0)
	{
		if (errno != ECONNRESET)
		{
			perror("recv");
		}
		removeFD(fd);
		return;
	}
	else if (!bytesRead)
	{
		removeFD(fd);
		return;
	}
	buf[bytesRead] = 0;

	file = fdopen(fd, "w");
	if (!file)
	{
		char *err = "fdopen failed\n";
		send(fd, err, strlen(err), 0);
		removeFD(fd);
		return;
	}

	table[fd]->file = file;

	if (strstr((char *) buf, "/exit"))
	{
		char *goodbye =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
			"Bye!"
		;
		fprintf(file, goodbye);
		removeFD(fd);
		exit(0);
	}
	else if (strstr((char *) buf, "/times"))
	{
		char *begin =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
		;

		fprintf(file, begin);
		printTimes(file);
		removeFD(fd);
	}
	else
	{
		char *hello =
			"HTTP/1.0 200 OK\n"
			"Content-Type: text/html\n"
			"\n"
		;

		fprintf(file, hello);

		fprintf(file, "<table>\n");

		fprintf(file, "<tr bgcolor=#cccccc>");
		fprintf(file, "<td>Time</td>");
		fprintf(file, "<td>Message</td>");
		fprintf(file, "<td>File</td>");
		fprintf(file, "<td>Line</td>");
		fprintf(file, "</tr>");

		for (i = 0; i < numberOfSlots; i++)
		{
			sortedStatusEntries[i] = i;
		}

		qsort(sortedStatusEntries, numberOfSlots, sizeof(int),
			compareStatusEntries);

		for (i = 0; i < numberOfSlots; i++)
		{
			StatusEntry	*entry;

			entry = &statusEntries[sortedStatusEntries[i]];
			fprintf(file, "<tr>");
			fprintf(file, "<td>%s</td>", ctime(&entry->time));
			fprintf(file, "<td>%s</td>",
				entry->message ? entry->message : "NULL");
			fprintf(file, "<td>%s</td>",
				entry->file ? entry->file : "NULL");
			fprintf(file, "<td>%d</td>", entry->line);
			fprintf(file, "</tr>");
		}

		fprintf(file, "</table>\n");

		removeFD(fd);
	}
}

static void
acceptNewClient(int fd)
{
	FD	*f;
	int	newFD;

	newFD = netAccept(fd);
	if (newFD < 0)
	{
		fprintf(stderr, "netAccept failed\n");
		return;
	}

	f = addFD(newFD, readClientRequest);
	if (!f)
	{
		fprintf(stderr, "addFD failed\n");
		return;
	}
}

static void *
startStatusFunc(void *a)
{
	FD	*f;
	int	fd;
	fd_set	localFDSet;
	int	ret;

	fd = netListen(NULL, NULL, &mainPort);
	if (fd < 0)
	{
		fprintf(stderr, "netListen failed\n");
		exit(0);
	}

	f = addFD(fd, acceptNewClient);
	if (!f)
	{
		fprintf(stderr, "addFD failed\n");
		exit(0);
	}

	while (1)
	{
		localFDSet = fdSet;
		ret = select(maxFD + 1, &localFDSet, NULL, NULL, NULL);
		if (ret == -1)
		{
			perror("select");
		}
		for (fd = 0; fd <= maxFD; fd++)
		{
			if (FD_ISSET(fd, &localFDSet))
			{
				(*table[fd]->handler)(fd);
			}
		}
	}

	return NULL;
}

static void
startStatusThread(void)
{
	int	ret;

	ret = threadCreate(&statusThread, startStatusFunc, NULL);
	if (ret)
	{
		exit(0);
	}
}

int
main(int argc, char *argv[])
{
	int		i;
	time_t		theTime;
	URL		*url;
	int		ret;

#if !defined(WINDOWS)
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		fprintf(stderr, "signal failed\n");
		exit(0);
	}
#endif

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d"))
		{
			i++;
			limitDomains[limitDomainsIndex] = argv[i];
			limitDomainsIndex++;
		}
		else if (!strcmp(argv[i], "-o"))
		{
			i++;
			outFile = argv[i];
		}
		else if (!strcmp(argv[i], "-s"))
		{
			i++;
			firstURL = argv[i];
		}
	}

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	time(&theTime);
	startTime = copyString((unsigned char *) ctime(&theTime));

#ifdef ROBOT_LOG_ATTRIBUTES
	attributeTable = hashAlloc(freeEntry);
#endif
	contentTypeTable = hashAlloc(freeEntry);
	httpCharsetTable = hashAlloc(freeEntry);
	httpHeaderTable = hashAlloc(freeEntry);
	metaCharsetTable = hashAlloc(freeEntry);
	schemeTable = hashAlloc(freeEntry);
#ifdef ROBOT_LOG_TAGS
	tagTable = hashAlloc(freeEntry);
#endif
	urlTable = hashAlloc(NULL);

	initKnownBadTags();

	addURLInit(addURLFunc, NULL, limitDomains);

	htmlRegister("meta", "content", metaHandler);
	htmlRegisterTagHandler(tagHandler);

	startStatusThread();

	for (i = 0; i < numberOfSlots; i++)
	{
		ret = threadCreate(&slots[i], startHere, (void *) i);
		if (ret)
		{
			exit(0);
		}
	}
	threadYield();

	if (!firstURL)
	{
		firstURL = "http://mozilla.org/";
	}

	url = urlParse((unsigned char *) firstURL);
	hashAdd(urlTable, (unsigned char *) firstURL, 0);
	urls = url;
	lastURL = url;
	currURL = url;
	threadCondSignal();
	threadYield();
	while (1)
	{
		if (allThreadsAreWaiting())
		{
			for (i = 0; i < numberOfSlots; i++)
			{
				threadCancel(slots[i]);
			}
			break;
		}
		if (count >= LIMIT)
		{
			threadCondBroadcast();
			for (i = 0; i < numberOfSlots; i++)
			{
				threadJoin(slots[i]);
			}
			threadCancel(statusThread);
			break;
		}
	}

	printStats();

#ifdef ROBOT_LOG_ATTRIBUTES
	hashFree(attributeTable);
#endif
	hashFree(contentTypeTable);
	hashFree(httpCharsetTable);
	hashFree(httpHeaderTable);
	hashFree(metaCharsetTable);
	hashFree(schemeTable);
#ifdef ROBOT_LOG_TAGS
	hashFree(tagTable);
#endif
	hashFree(urlTable);

	url = urls;
	while (url)
	{
		URL	*tmp;

		tmp = url;
		url = url->next;
		urlFree(tmp);
	}

	exit(0);
	return 1;
}
