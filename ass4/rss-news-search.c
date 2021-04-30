#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "bool.h"
#include "html-utils.h"
#include "streamtokenizer.h"
#include "url.h"
#include "vector.h"
#include "hashset.h"

typedef struct {
  char* title;
  char* url;
  char* server;
  int frequency;
} article;

typedef struct {
  char* word;
  vector* articles;
} entry;

static void Welcome(const char *welcomeTextFileName);
static void BuildIndices(const char *feedsFileName, hashset* entryListPtr, hashset* stopListPtr);
static void ProcessFeed(const char *remoteDocumentName, hashset* entryListPtr, hashset* stopListPtr);
static void PullAllNewsItems(FILE *dataStream, hashset* entryListPtr, hashset* stopListPtr);
static bool GetNextItemTag(streamtokenizer *st);
static void ProcessSingleNewsItem(streamtokenizer *st, hashset* entryListPtr, hashset* stopListPtr);
static void ExtractElement(streamtokenizer *st, const char *htmlTag,
                           char dataBuffer[], int bufferLength);
static void ParseArticle(const char *articleTitle,
                         const char *articleDescription,
                         const char *articleURL, 
                         hashset* entryListPtr,
                         hashset* stopListPtr);
static void ScanArticle(streamtokenizer *st, const char *articleTitle,
                        const char *unused, const char *articleURL, 
                        hashset* entryListPtr, hashset* stopListPtr,
                        bool isLocalAddress);
static void QueryIndices(hashset* entryListPtr, hashset* stopListPtr);
static void ProcessResponse(const char *word, hashset* entryListPtr, hashset* stopListPtr);
static bool WordIsWellFormed(const char *word);
static void buildStopList(hashset* stopList);

static const signed long kHashMultiplier = -1664117991L;

/**
 * Function: StringHash
 * -----------------
 * Used as hash function of hashset of string
*/
static int StringHash(const void *elemAddr, int numBuckets)  
{       
  char* s = *(char**)elemAddr;     
  int i;
  unsigned long hashcode = 0;
  
  for (i = 0; i < strlen(s); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  return hashcode % numBuckets;                                
}

/**
 * Function: EntryHash
 * -----------------
 * Used as hash function of hashset of entries
 * Generates hash value with word
*/
static int EntryHash(const void *elemAddr, int numBuckets)  
{        
  char* s = ((entry*)elemAddr)->word;
  int i;
  unsigned long hashcode = 0;
  
  for (i = 0; i < strlen(s); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  return hashcode % numBuckets;                                
}

/**
 * Function: FreeString
 * -----------------
 * Used as free function of hashset of strings
*/
static void FreeString(void* elemAddr) {
  free((*(char**)elemAddr));
}

/**
 * Function: FreeEntry
 * -----------------
 * Used as free function of hashset of entries
*/
static void FreeEntry(void* elemAddr) {
  free(((entry*)elemAddr)->word);
  VectorDispose(((entry*)elemAddr)->articles);
  free(((entry*)elemAddr)->articles);
}

/**
 * Function: FreeArticle
 * -----------------
 * Used as free function of vector of articles
*/
static void FreeArticle(void* elemAddr) {
  free(((article*)elemAddr)->title);
  free(((article*)elemAddr)->url);
  free(((article*)elemAddr)->server);
}

/**
 * Function: StringCmp
 * -----------------
 * Used as compare function of hashset of strings
*/
static int StringCmp(const void* a, const void* b) {
  char* s1 = *(char**)a;
  char* s2 = *(char**)b;
  return strcasecmp(s1, s2);
}

/**
 * Function: EntryCmp
 * -----------------
 * Used as compare function of hashset of entries
*/
static int EntryCmp(const void* a, const void* b) {
  char* s1 = ((entry*)a)->word;
  char* s2 = ((entry*)b)->word;
  return strcasecmp(s1, s2);
}

/**
 * Function: SortCmp
 * -----------------
 * Used as compare function of vector of articles
*/
static int ArticleCmp(const void* a, const void* b) {
  article* a1 = (article*)a;
  article* a2 = (article*)b;
  if (a1->url == a2->url) return 0;
  if (a1->server != NULL && a2->server != NULL) {
    if (a1->server == a2->server && a1->title == a2->title) return 0;
  }
  return strcmp(a1->title, a2->title); 
}

/**
 * Function: SortCmp
 * -----------------
 * Used for sorting vector of articles
*/
int SortCmp(const void* ptr1, const void* ptr2){
  article* a1 = (article*)ptr1;
  article* a2 = (article*)ptr2;
  return(a2->frequency - a1->frequency);
}

/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to
 * map words to the collection of news articles where that
 * word appears.
 */

static const char *const kWelcomeTextFile = "data/welcome.txt";
static const char *const kDefaultFeedsFile = "data/test.txt";
static const char *const kFilePrefix = "file://";
static const char *const kTextDelimiters =
    " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static const int stopListBuckets = 1009;
static const int wordListBuckets = 10007;
static const int vectorInitialAllocation = 10;
static const char *const KStopWordsFile = "data/stop-words.txt";

int main(int argc, char **argv) {
  
  hashset stopList;
  hashset entryList;
  HashSetNew(&entryList, sizeof(entry), wordListBuckets, EntryHash, EntryCmp, FreeEntry);
  HashSetNew(&stopList, sizeof(char*), stopListBuckets, StringHash, StringCmp, FreeString);
  buildStopList(&stopList);
  setbuf(stdout, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  Welcome(kWelcomeTextFile);
  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], &entryList, &stopList);
  QueryIndices(&entryList, &stopList); 
  HashSetDispose(&stopList);
  HashSetDispose(&entryList);
  curl_global_cleanup();
  return 0;
}

size_t SavePage(char *ptr, size_t size, size_t nmemb, void *data) {
  return fprintf((FILE *)data, "%s", ptr);
}

static FILE *RemoveCData(const char *tmpFile) {
  FILE *inp = fopen(tmpFile, "rb");
  fseek(inp, 0, SEEK_END);
  long fsize = ftell(inp);
  fseek(inp, 0, SEEK_SET); /* same as rewind(f); */
  char *contents = malloc(fsize + 1);
  long read = fread(contents, 1, fsize, inp);
  assert(fsize == read);
  fclose(inp);
  FILE *out = fopen(tmpFile, "w");
  bool inside_cdata = false;
  for (int i = 0; i < fsize; ++i) {
    if (strncasecmp(contents + i, "<![CDATA[", strlen("<![CDATA[")) == 0) {
      inside_cdata = true;
      i += strlen("<![CDATA[") - 1;
    } else if (inside_cdata && strncmp(contents + i, "]]>", 3) == 0) {
      inside_cdata = false;
      i += 2;
    } else {
      fprintf(out, "%c", contents[i]);
    }
  }
  fclose(out);
  free(contents);
  return fopen(tmpFile, "r");
}

static FILE *FetchURL(const char *path, const char *tmpFile) {
  FILE *tmpDoc = fopen(tmpFile, "w");
  CURL *curl;
  CURLcode res;
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_URL, path);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SavePage);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, tmpDoc);
  res = curl_easy_perform(curl);
  fclose(tmpDoc);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    return NULL;
  }
  return RemoveCData(tmpFile);
}

static const char *const kNewLineDelimiters = "\r\n";
static void buildStopList(hashset* stopList) {
  FILE *infile;
  streamtokenizer st;
  char buffer[1024];

  infile = fopen(KStopWordsFile, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    char* str = strdup(buffer);
    HashSetEnter(stopList, &str);
  }
  STDispose(&st); 
  fclose(infile);
}

/**
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */

static void Welcome(const char *welcomeTextFileName) {
  FILE *infile;
  streamtokenizer st;
  char buffer[1024];
  infile = fopen(welcomeTextFileName, "r");
  assert(infile != NULL);

  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    printf("%s\n", buffer);
  }

  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew
                  // doesn't open one..
  fclose(infile);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of
 * indices. Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name
 * (it's in the file for humans to read, but our aggregator doesn't care what
 * the name is) and then extracts the URL.  It then relies on ProcessFeed to
 * pull the remote document and index its content.
 */

static void BuildIndices(const char *feedsFileName, hashset* entryListPtr, hashset* stopListPtr) {
  FILE *infile;
  streamtokenizer st;
  char remoteFileName[1024];
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STSkipUntil(&st, ":") !=
         EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(
        &st,
        ": "); // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteFileName, sizeof(remoteFileName));
    ProcessFeed(remoteFileName, entryListPtr, stopListPtr);
  }

  STDispose(&st);
  fclose(infile);
  printf("\n");
}

/** * Function: ProcessFeedFromFile * --------------------- * ProcessFeed
 * locates the specified RSS document, from locally */

static void ProcessFeedFromFile(char *fileName, hashset* entryListPtr, hashset* stopListPtr) {
  FILE *infile; 
  streamtokenizer st;
  char articleDescription[1024];
  articleDescription[0] = '\0';
  infile = fopen((const char *)fileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kTextDelimiters, true);
  ScanArticle(&st, (const char *)fileName, articleDescription,
              (const char *)fileName, entryListPtr, stopListPtr, true);
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew
                  // doesn't open one..
  fclose(infile);
}

/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly
 * redirected) connection to that remote document can be established, then
 * PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and
 * inspect the documentation for ParseArticle for information about what the
 * different response codes mean.
 */

static void ProcessFeed(const char *remoteDocumentName, hashset* entryListPtr, hashset* stopListPtr) {

  if (!strncmp(kFilePrefix, remoteDocumentName, strlen(kFilePrefix))) {
    ProcessFeedFromFile((char *)remoteDocumentName + strlen(kFilePrefix), entryListPtr, stopListPtr);
    return;
  }
  FILE *tmpFeed = FetchURL(remoteDocumentName, "tmp_feed");
  PullAllNewsItems(tmpFeed, entryListPtr, stopListPtr);
  fclose(tmpFeed);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the
 * names and URLs of online news articles.  Check out
 * "datafiles/sample-rss-feed.txt" for an idea of what an RSS feed from the
 * www.nytimes.com (or anything other server that syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where
 * each item is detailed using a generalization of HTML called XML.  A typical
 * XML fragment for a single news item will certainly adhere to the format of
 * the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important
 * moment in the transformation of Benedict XVI.</description> <author>By IAN
 * FISHER and LAURIE GOODSTEIN</author> <pubDate>Sun, 24 Apr 2005 00:00:00
 * EDT</pubDate> <guid
 * isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * PullAllNewsItems reads and discards all characters up through the opening
 * <item> tag (discarding the <item> tag as well, because once it's read and
 * indentified, it's been pulled,) and then hands the state of the stream to
 * ProcessSingleNewsItem, which handles the job of pulling and analyzing
 * everything up through and including the </item> tag. PullAllNewsItems
 * processes the entire RSS feed and repeatedly advancing to the next <item> tag
 * and then allowing ProcessSingleNewsItem do process everything up until
 * </item>.
 */

static void PullAllNewsItems(FILE *dataStream, hashset* entryListPtr, hashset* stopListPtr) {
  streamtokenizer st;
  STNew(&st, dataStream, kTextDelimiters, false);
  while (GetNextItemTag(
      &st)) { // if true is returned, then assume that <item ...> has just been
              // read and pulled from the data stream
    ProcessSingleNewsItem(&st, entryListPtr, stopListPtr);
  }

  STDispose(&st);
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.
 *
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 */

static const char *const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer *st) {
  char htmlTag[1024];
  while (GetNextTag(st, htmlTag, sizeof(htmlTag))) {
    if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * Function: ProcessSingleNewsItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML
 * feed. At the moment this function is called, we're to assume that the <item>
 * tag was just read and that the streamtokenizer is currently pointing to
 * everything else, as with:
 *
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and
 * 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>,
 * storing the title, link, and article description in local buffers long enough
 * so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in
 * any particular order.  We do asssume that the link field exists (although we
 * can certainly proceed if the title and article descrption are missing.) There
 * are often other tags inside an item, but we ignore them.
 */

static const char *const kItemEndTag = "</item>";
static const char *const kTitleTagPrefix = "<title";
static const char *const kDescriptionTagPrefix = "<description";
static const char *const kLinkTagPrefix = "<link";
static void ProcessSingleNewsItem(streamtokenizer *st, hashset* entryListPtr, hashset* stopListPtr) {
  char htmlTag[1024];
  char articleTitle[1024];
  char articleDescription[1024];
  char articleURL[1024];
  articleTitle[0] = articleDescription[0] = articleURL[0] = '\0';
  while (GetNextTag(st, htmlTag, sizeof(htmlTag)) &&
         (strcasecmp(htmlTag, kItemEndTag) != 0)) {
    if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0) {
      ExtractElement(st, htmlTag, articleTitle, sizeof(articleTitle));
    }
    if (strncasecmp(htmlTag, kDescriptionTagPrefix,
                    strlen(kDescriptionTagPrefix)) == 0)
      ExtractElement(st, htmlTag, articleDescription,
                     sizeof(articleDescription));
    if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0)
      ExtractElement(st, htmlTag, articleURL, sizeof(articleURL));
  }

  if (strncmp(articleURL, "", sizeof(articleURL)) == 0)
    return; // punt, since it's not going to take us anywhere
  ParseArticle(articleTitle, articleDescription, articleURL, entryListPtr, stopListPtr);
}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching
 * end tag.  It assumes that the most recently extracted HTML tag resides in the
 * buffer addressed by htmlTag.  The implementation populates the specified data
 * buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.
 * Assuming for illustration purposes that htmlTag addresses a buffer containing
 * "<description" followed by other text, these three scanarios are handled:
 *
 *    Normal Situation:
 * <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.
 * This is not uncommon for the description data to be missing, so we need to
 * cover all three scenarious (I've actually seen all three.) It would be quite
 * unusual for the title and/or link fields to be empty, but this handles those
 * possibilities too.
 */

static void ExtractElement(streamtokenizer *st, const char *htmlTag,
                           char dataBuffer[], int bufferLength) {
  assert(htmlTag[strlen(htmlTag) - 1] == '>');
  if (htmlTag[strlen(htmlTag) - 2] == '/')
    return; // e.g. <description/> would state that a description is not being
            // supplied
  STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "<");
  RemoveEscapeCharacters(dataBuffer);
  if (dataBuffer[0] == '<')
    strcpy(dataBuffer, ""); // e.g. <description></description> also means
                            // there's no description
  STSkipUntil(st, ">");
  STSkipOver(st, ">");
}

/**
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the
 * three parameters.  The network connection is either established of not.  The
 * implementation is prepared to handle a subset of possible (but by far the
 * most common) scenarios, and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be
 * contacted. 200 means that the document exists and that a connection to that
 * very document has been established. 301 means that the document has moved to
 * a new location 302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist
 * (404), or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them,
 * since no others appears all that often, and it'd be tedious to be fully
 * exhaustive in our enumeration of all possibilities.
 */

static void ParseArticle(const char *articleTitle,
                         const char *articleDescription,
                         const char *articleURL,
                         hashset* entryListPtr, 
                         hashset* stopListPtr) {
  FILE *tmpDoc = FetchURL(articleURL, "tmp_doc");
  if (tmpDoc == NULL) {
    printf("Unable to fetch URL: %s\n", articleURL);
    return;
  }
  printf("Scanning \"%s\"\n", articleTitle);
  streamtokenizer st;
  STNew(&st, tmpDoc, kTextDelimiters, false);
  ScanArticle(&st, articleTitle, articleDescription, articleURL, entryListPtr, stopListPtr, false);
  STDispose(&st);
  fclose(tmpDoc);
}

/**
 * Function: ScanArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the
 * numbers of well-formed words that could potentially serve as keys in the set
 * of indices. Once the full article has been scanned, hashset of indices is ready
 */

static void ScanArticle(streamtokenizer *st, const char *articleTitle,
                        const char *unused, const char *articleURL,
                        hashset* entryListPtr, hashset* stopListPtr, bool isLocalAddress) {
  char word[1024];

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word); 
      char* str = strdup(word);
      if (WordIsWellFormed(word) && HashSetLookup(stopListPtr, &str) == NULL) {
        entry key;
        key.word = strdup(word);
        key.articles = NULL;

        article toAdd;
        toAdd.frequency = 1;
        toAdd.title = strdup(articleTitle);
        toAdd.url = strdup(articleURL);

        if (isLocalAddress) {
          toAdd.server = NULL;
        } else {
          url u;
          URLNewAbsolute(&u, articleURL);
          toAdd.server = strdup(u.serverName);
          URLDispose(&u);
        }

        if (HashSetLookup(entryListPtr, &key) == NULL) { //If hashset doesn't contain word
          vector* v = malloc(sizeof(vector));
          VectorNew(v, sizeof(article), FreeArticle, vectorInitialAllocation);
          VectorAppend(v, &toAdd);
          key.articles = v;
          HashSetEnter(entryListPtr, &key);
        } else { //If hashset contains word
          entry* entryAddr = (entry*)HashSetLookup(entryListPtr, &key); 
          int index = VectorSearch(entryAddr->articles, &toAdd, ArticleCmp, 0, false);
          if (index < 0) { //If vector of articles doesn't contain article
            VectorAppend(entryAddr->articles, &toAdd);
          } else { //If vector of articles  contains article
            ((article*)VectorNth(entryAddr->articles, index))->frequency++; 
            free(toAdd.title);
            free(toAdd.url);
            free(toAdd.server);
          }
          free(key.word);
        }
      }
      free(str);
    }
  }
}

/**
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to articles (sorted by
 * relevance) that contain that word.
 */

static void QueryIndices(hashset* entryListPtr, hashset* stopListPtr) {
  char response[1024];
  while (true) {
    printf("Please enter a single search term [enter to break]: "); 
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) {
      break;
    }
    ProcessResponse(response, entryListPtr, stopListPtr);
  }
}

/**
 * Function: ProcessResponse
 * -------------------------
 * Proccesses every word inputed by user
 */

static void ProcessResponse(const char *word, hashset* entryListPtr, hashset* stopListPtr) {
  if (HashSetLookup(stopListPtr, &word) != NULL) {
    printf("Too common a word to be taken seriously. Try something more specific.\n");
    return;
  }
  if (WordIsWellFormed(word)) {
    entry e;
    e.articles = NULL;
    e.word = strdup(word);
    entry* entryAddr = (entry*)HashSetLookup(entryListPtr, &e);
    if (entryAddr != NULL) {        
      VectorSort(entryAddr->articles, SortCmp);
      for (int i = 0; i < VectorLength(entryAddr->articles); i++) {
        article* art = (article*)VectorNth(entryAddr->articles, i);
        if (art->frequency == 1) {
          printf("%d.) \"%s\" [search term occurs %d time]\n \"%s\"\n", i + 1, art->title, art->frequency, art->url);
        } else {
          printf("%d.) \"%s\" [search term occurs %d times]\n \"%s\"\n", i + 1, art->title, art->frequency, art->url);
        }
      }
    } else {
      printf("None of today's news articles contain the word \"%s\".\n", word);
    }
    free(e.word);
  } else {
    printf(
        "\tWe won't be allowing words like \"%s\" into our set of indices.\n",
        word);
  }
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.
 */

static bool WordIsWellFormed(const char *word) {
  int i;
  if (strlen(word) == 0)
    return true;
  if (!isalpha((int)word[0]))
    return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int)word[i]) && (word[i] != '-'))
      return false;

  return true;
}
