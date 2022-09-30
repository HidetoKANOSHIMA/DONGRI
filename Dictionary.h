#ifndef __DICTIONARY__
#define __DICTIONARY__

#define   DICTIONARY_SIZE     (1024L * 40L)
#define   WORDNAME_MAX        (255)
struct    DICTIONARY{
  OBJECT          *newestWordPtr;
  OBJECT          *nextObjectPtr;
  char            nameBuf[WORDNAME_MAX + 1];
  OBJECT          body[DICTIONARY_SIZE];
  int             endp;
};

void    initDictionary();
OBJECT        *getNextObjectPtr();
D_WORD_HEADER *searchDictionary(const char *name);
D_WORD_HEADER *searchDictionaryIncludeMe(const char *name);
D_WORD_HEADER *searchDictionaryWithError(const char *name);
void          forgetDictionary(D_WORD_HEADER *dWordHeaderForget);
void          prepareUnnamedCompile();
void          prepareNamedCompile(const char *name);
void          setupPrimitiveWord(PRIMITIVE_WORD_INFO *wInfo);
unsigned int  dumpDictionary(const char *searchSub);
boolean       updateUsage(const char *wordName, const char *newComment);
void          updateNewestWordPtr(D_WORD_HEADER *dWordHeader);
void          prepareCompile(const char *wordName, boolean immediateFlag, D_WORD_OBJECT_PRIMITIVE primitiveFunc);
boolean       finishCompile();
void          compileStandardWord(D_WORD_HEADER *dWordHeader);
void          compileObject(OBJECT data);
void          compileInt(int data);
void          compileFloat(float data);
void          compileVar();
void          compileDo();
void          compileLoop();
void          compilePLoop();
void          compileJump();
void          compileIfJump();
void          compileFIfJump();
void          dumpWord(D_WORD_HEADER *dWordHeader);
boolean       isNamedHeader(D_WORD_HEADER * dWordHeader);
boolean       isNewestHeaderIsNamedHeader();
boolean       isNewestWordEmpty();
boolean       compileNumber(const char *name);
boolean       compileString(const char *name);
//
void          approvalDump(boolean app);
void          approvalRUN(boolean app);
int           dictionaryFree();


#endif
