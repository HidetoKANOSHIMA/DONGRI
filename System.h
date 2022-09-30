#ifndef __SYSTEM__
#define __SYSTEM__

union OBJECT{
  int           num;
  unsigned int  unum;
  float         flt;
  void          *ptr;
};

struct  STACK_ITEM{
  unsigned  short   attr;
  OBJECT            value;
};
enum    STACK_ATTR{   // スタック上データの属性。構文スタックも他とおなじ構造なので、構文識別子もここに入れた。
  ATTR_UNKNOWN = 0,
  ATTR_INTNUM = 1, ATTR_FLOATNUM = 2, ATTR_STRING = 3,
  ATTR_CHAR = 4, ATTR_STRINGREF = 5, ATTR_ARRAYREF = 6,
  ATTR_SYNTAX_DO = 128, ATTR_SYNTAX_IF = 129, ATTR_SYNTAX_BEGIN = 130, ATTR_SYNTAX_WHILE = 131
};

struct  D_WORD_OBJECT;

typedef D_WORD_OBJECT*          D_WORD_OBJECT_FORTH;
typedef D_WORD_OBJECT_FORTH*    FORTH_IP;
typedef FORTH_IP(*D_WORD_OBJECT_PRIMITIVE)(FORTH_IP);

struct  PRIMITIVE_WORD_INFO{
  const   char            *dWordName;
  bool                    immediateFlag;
  D_WORD_OBJECT_PRIMITIVE primitiveFunc;
  const        char       *dWordComment;
};

struct  D_WORD_OBJECT{
  D_WORD_OBJECT_PRIMITIVE primitiveFunc;
  D_WORD_OBJECT_FORTH     dObject[0];
};

struct  D_WORD_HEADER{
  D_WORD_HEADER   *dPrevWordHeader;
  bool            immediateFlag;
  const char      *dWordName;
        char      *dWordComment;
  D_WORD_OBJECT   object;
};

void    initSystem();
void    setCompilerWaitName();
void    resetCompilerWaitName();
boolean isCompilerWaitName();
boolean flushNewestWord();
void    putPrompt();
boolean FORTH_RUN(D_WORD_OBJECT_FORTH ip);
boolean forthEntry(const char *name);


#endif
