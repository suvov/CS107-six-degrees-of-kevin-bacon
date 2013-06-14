using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"
#include <stdlib.h>
#include <cstring>
#include <string>


const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

// sets of valid offsets
set<int> validActor;
set<int> validMovie;

// prototypes
void getMovies(void * actorPt, vector<film> &films, const void * movieFile);
void getActors(void * moviePt, vector<string> &players, const void * actorFile);
void parseMovieRecord(char * movie, vector<film> &films);
void parseActorRecord(char * actor, vector<string> &players);
void NextField(char * &ptr);
film MakeFilm (char * &movie);
string toString(char * &cstr);
bool isValidOffsetActor(int offset);
bool isValidOffsetMovie(int offset);
void constructSets(const void * actorFile, const void * movieFile);
void constructActorSet(const void * actorFile);
void constructMovieSet(const void * movieFile);

// comparison function for getCredits()
int CmpFunc1(const void * k_struct, const void * actorOffset) 
{
	int * offset = (int *) actorOffset; // integer value for offset
	keyT * temp =  (keyT *) k_struct; 
	const void * actorFile = temp->file; // taking actorFile out in order to use it
	char * key = temp->key; 
	char * actor = (char *)actorFile + *offset; // making c_string for actor to be compared
	return strcmp(key, actor); // using built-in c_string comparison function
}
// comparison function for getCast()
int CmpFunc2(const void * k_struct, const void * movieOffset)
{
	keyT * temp = (keyT *) k_struct;
	const void * movieFile = temp->file;
	int * offset = (int *) movieOffset;
	char * movie = (char *)movieFile + *offset;
	film keyMovie = *temp->movie;	
	film currentMovie = MakeFilm(movie);
	if(keyMovie == currentMovie) return 0;
	else if(keyMovie < currentMovie) return -1;
	else return 1;	
}

imdb::imdb(const string& directory)
{
	const string actorFileName = directory + "/" + kActorFileName;
	const string movieFileName = directory + "/" + kMovieFileName;
	  
	actorFile = acquireFileMap(actorFileName, actorInfo);
	movieFile = acquireFileMap(movieFileName, movieInfo);
	constructSets(actorFile, movieFile); // sets of valid offsets(because data files partly broken)  
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

bool imdb::getCredits(const string& player, vector<film>& films) const 
{ 	
	char * key = new char[player.size()+1];
	strcpy(key, player.c_str());
	void * base = (int *)actorFile +1; // address of offset to first actor record
	int n = *(int *) actorFile; // number of actor records
	int elemSize = sizeof(int); // 
	keyT k_struct; // struct with actorFile and key
	k_struct.file = actorFile;
	k_struct.key = key;
	// returns offset to actor's record in actorFile or NULL if record wasn't found
	int * offset = (int *) bsearch(&k_struct, base, n, elemSize, CmpFunc1); 
	void * actor;		
	if(offset != NULL) {
		actor = (char *) actorFile + *offset; // actor's record address	
		getMovies(actor, films, movieFile);
		return true;
	} else {
		return false;
	}
}

bool imdb::getCast(const film& movie, vector<string>& players) const 
{ 
	keyT k_struct;
	k_struct.file = movieFile;
	k_struct.movie = &movie;
	void * base = (int *) movieFile +1; // addr of first offset value
	int n = *(int *) movieFile; // no of movie records
	int elemSize = sizeof(int);
	int * offset = (int *) bsearch(&k_struct, base, n, elemSize, CmpFunc2);
	void * moviePt;
	if(offset !=NULL) {
		moviePt = (char *) movieFile + *offset; // movie's record address
		getActors(moviePt, players, actorFile);		
		return true;
	}else {
		cout <<"wasn't found" <<endl;
		return false;
	} 
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// populate vector of films with films for given actor
void getMovies(void * actorPt, vector<film> &films, const void * movieFile)
{
	char * ptr = (char *) actorPt;
	NextField(ptr);
	short n = *(short *) ptr; // number of movies
	NextField(ptr);
	char * movie; 
	int * offset = (int *) ptr; // offset for movieFile
	for(int i = 0; i < n; i++){ // for each
		if(isValidOffsetMovie(*offset)) {
			movie = (char *) movieFile + *offset; // movie record address
			parseMovieRecord(movie, films); // 
		}
		offset++; // increments using standard ptr arythmetic (4 bytes)
	}
}

//
void getActors(void * moviePt, vector<string> &players, const void * actorFile)
{
	char * movie = (char *) moviePt;
	NextField(movie);
	movie++; //skip year value;
	if(*movie == '\0') movie++; // if there is an additional zero, skip it
	short n = *(short *) movie; // number of actors
	NextField(movie);
	char * actor;
	int * offset = (int *) movie; // offset for actorFile
	for(int  i = 0; i < n; i++) {
		// need to check if offset is correct(because data files are bad)
		if(isValidOffsetActor(*offset)) { 
			actor = (char *) actorFile + *offset; // movie record address	
			parseActorRecord(actor, players); // 
		}
		offset++; // increments using standard ptr arythmetic (4 bytes)
	}	
}

//
void parseMovieRecord(char * movie, vector<film> &films)
{
	film current = MakeFilm(movie);
	films.push_back(current);	
}

//
void parseActorRecord(char * actor, vector<string> &players)
{
	string name = toString(actor);
	players.push_back(name);		
}

//
void NextField(char * &ptr)
{	
	while(*ptr != '\0') ptr++; // eat !zeros
	while(*ptr == '\0') ptr++; // eat zeros
}

//
film MakeFilm (char * &movie)
{	
	string title = toString(movie);
	NextField(movie);
	char ch = *movie;
	int year = (int) ch + 1900;
	film current;
	current.title = title;
	current.year = year;
	return current;
}

//
string toString(char * &cstr)
{
	string result;
	while(true) {
		if(*cstr == '\0') break;
		result += *cstr;
		cstr++;			
	}
	return result;
}

// 
void constructSets(const void * actorFile, const void * movieFile)
{
	constructActorSet(actorFile);
	constructMovieSet(movieFile);
	
}

//
void constructActorSet(const void * actorFile)
{
	int * base = (int *)actorFile +1; // address of offset to first actor record
	int n = *(int *) actorFile; // number of actor records
	for(int i = 0; i < n; i++) {
		validActor.insert(*base);
		base++;		
	}
}

//
void constructMovieSet(const void * movieFile)
{
	int * base = (int *)movieFile +1; // address of offset to first movie record
	int n = *(int *) movieFile; // number of movie records
	for(int i = 0; i < n; i++) {
		validMovie.insert(*base);
		base++;		
	}	
}

//	
bool isValidOffsetActor(int offset) { return validActor.find(offset) != validActor.end(); }

//
bool isValidOffsetMovie(int offset) { return validMovie.find(offset) != validMovie.end(); }


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
