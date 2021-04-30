using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"
#include <string.h>

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

//For binary search
struct film_key {
  const void* movie;
  const void* start;
};

//For binary search
struct actor_key {
  const void* name;
  const void* start;
};

imdb::imdb(const string& directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

//compare function for player strings
int player_cmp(const void* a, const void* b) {
  actor_key key = *(actor_key*)a;
  char* s1 = (char*)key.name; // pointer to first character of target string
  int offset = *(int*)b;
  char* s2 = (char*)key.start + offset; // pointer to first character of string to compare
  return strcmp(s1, s2);
}

//compare function for film struct
int movie_cmp(const void* a, const void* b) {
  film_key key = *(film_key*)a;
  film* to_find = (film*)key.movie;
  int offset = *(int*)b; 
  char* ch = (char*)key.start + offset; // pointer to first byte of filmrecord
  string s = "";
  int year = 1900;
  while(*ch != '\0') {
    s += *ch;
    ch++;
  }
  ch++; // skip '\0 character'

  year += *(char*)ch;
  film f;
  f.title = s;
  f.year = year;

  //compare film structs
  if (f == *to_find) return 0;
  if (f < *to_find) return 1;
  return -1;
}


// you should be implementing these two methods right here... 
bool imdb::getCredits(const string& player, vector<film>& films) const { 
  int n_actors = *(int*)actorFile; // get number of players
  
  //make actor_key struct
  actor_key key;
  key.name = (void*)player.c_str();
  key.start = actorFile;
  
  void* actor = bsearch(&key, (char*)actorFile + sizeof(int), n_actors, sizeof(int), player_cmp);
  
  //if not found return false;
  if (actor == NULL) return false;

  int offset_from_start = *(int*)actor;

  //pointer to first byte of actor record
  char* ch = (char*)actorFile + offset_from_start;

  int offset = 0;

  //if string size is even move pointer by two bytes else by one byte
  if (player.length() % 2 == 0) { 
    ch += player.length() + sizeof(short);
    offset += player.length() + sizeof(short);
  } else {
    ch += player.length() + sizeof(char);
    offset += player.length() + sizeof(char);
  } 

  int n_films = *(short*)ch; //get number of films

  //move pointer by 2 bytes
  ch += sizeof(short);
  offset += sizeof(short);

  //if offset isn't divisible by 4 move pointer by 2 bytes
  if (offset % 4 != 0) ch += sizeof(short);

  //get films
  for (int i = 0; i < n_films; i++) {
    char* p = (char*)movieFile + *(int*)ch; 
    ch = (char*)ch + sizeof(int); 

    string title = "";
   
    //get title
    while(*p != '\0') {
      title += *p;
      p++;
    }
    p++; //skip '\0 character'

    int year = *p + 1900;

    struct film f;
    f.title = title;
    f.year = year;

    films.push_back(f);
  }
  return true; 
}

bool imdb::getCast(const film& movie, vector<string>& players) const {
  int n_movies = *(int*)movieFile; //number of movies

  film_key key;
  key.movie = (void*)&movie; 
  key.start = movieFile;
 
  void* movie_info = bsearch(&key, (char*)movieFile + sizeof(int), n_movies, sizeof(int), movie_cmp);
  
  //if not found return false;
  if (movie_info == NULL) return false;

  //get pointer to first byte of movierecord
  char* tmp = (char*)movieFile + *(int*)movie_info;
  int offset = 0;

  //skip movie title
  while(*tmp != '\0') {
    offset++;
    tmp++;
  }

  //skip '\0' char
  tmp++;
  offset++;

  /*if size of title and one byte together is odd move pointer by 2 bytes
  (1 byte for extra '\0' chacater and 1 byte for year)
  else move pointer by 1 byte (to skip byte in wich year is stored)*/
  if((offset + sizeof(char)) % 2 == 1) {
    tmp += 2 * sizeof(char);
    offset += 2 * sizeof(char);
  } else {
    tmp += sizeof(char);
    offset += sizeof(char);
  }

  int n_players = *(short*)tmp; 
  tmp += sizeof(short);
  offset += sizeof(short);

  // if offset isn't divisible by 4 move pointer by 2 bytes
  if (offset % 4 != 0) tmp += sizeof(short); 

  for (int i = 0; i < n_players; i++) {
    //pointer to first byte of record
    char* p = (char*)actorFile + *(int*)tmp;

    //move pointer by one integer size
    tmp += sizeof(int);

    //get player name
    string player = "";
    while(*p != '\0') {
      player += *p;
      p++;
    }
    players.push_back(player);
  }

  return false;
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
