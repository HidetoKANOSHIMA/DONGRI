#include    <arduino.h>
#include    "System.h"
#include    "StreamIO.h"
#include    "Dictionary.h"
#include    "Primitive.h"
#include    "Utility.h"

boolean     _approvalDump = true;
boolean     _approvalRUN  = true;
/*
 * ともに boolean  finishCompile() の中で作用するフラグとその制御。
 * _approvalRUNがfalseの場合はコンパイル即実行モードでも実行時インタプリタを呼び出さない。
 * _approvalDumpがfalseならばコンパイル後のオブジェクトダンプを抑止する。
 * なおこの設定は、FORTHワード"OBJDUMPON"ならびに"OBJDUMPOFF"で制御できる。
 */
void        approvalDump(boolean app){
  _approvalDump = app;
}
void        approvalRUN(boolean app){
  _approvalRUN = app;
}

/*
 * 辞書。
 */
DICTIONARY    dictionary;
/*
 * dictionary.newestWordPtrを更新する。
 */
void    updateNewestWordPtr(D_WORD_HEADER *dWordHeader){
  dictionary.newestWordPtr = (OBJECT *)dWordHeader;
}
/*
 * 辞書の初期化。
 */
void    initDictionary(){
  dictionary.body[0].num = 0;
  updateNewestWordPtr((D_WORD_HEADER *)dictionary.body);
  dictionary.nextObjectPtr = NULL;
}
/*
 * 実名ワードヘッダか否かを検査する。実名ワードヘッダならtrueを返す。
 * 構造体メンバを直接見ないで、必ずこれを呼び出すこと。
 */
boolean isNamedHeader(D_WORD_HEADER *dWordHeader){
  return  *(dWordHeader->dWordName) != '\0';
}
boolean isNewestHeaderIsNamedHeader(){
  return  isNamedHeader((D_WORD_HEADER *)dictionary.newestWordPtr);
}
/*
 * ワードヘッダを準備する。
 * wordNameは無名ワードヘッダの場合は空列("")が渡される。このこと以外に実名ワードヘッダとの違いはない。
 * このため、いわゆるインタプリタモードか否かは*(dWordHeader->dWordName)が'\0'であるか否かを検査することになる。
 */
void  prepareCompile(const char *wordName, boolean immediateFlag, D_WORD_OBJECT_PRIMITIVE primitiveFunc, const char *wordComment){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  strcpy(dictionary.nameBuf, wordName);         // ワード名を一時退避。この先はwordNameが指すメモリの内容は不要になる。
  dWordHeader->dWordName = dictionary.nameBuf;  // 無名コンパイルの場合は空列("")を指している。
  dWordHeader->immediateFlag = immediateFlag;
  dWordHeader->dWordComment = NULL;
  dWordHeader->object.primitiveFunc = primitiveFunc;
  dictionary.nextObjectPtr = (OBJECT *)&dWordHeader->object.dObject;  // これからワードをコンパイルしていくアドレス。
  // 以下、メモリ領域を確保してワードのコメントを格納する処理。無名ワードヘッダに対してはこの処理は行わない。
  if (isNamedHeader(dWordHeader)){
    int len = strlen(wordComment);
    char *ptr = (char *)malloc(len + 1);
    if (ptr != NULL){
      strcpy(ptr, wordComment);
      dWordHeader->dWordComment = ptr;
    }
  }
}
/*
 * 無名ワードヘッダを準備する。
 */
void  prepareUnnamedCompile(){
  D_WORD_OBJECT_PRIMITIVE primitiveFunc = searchDictionaryWithError(CW_PUSHIP)->object.primitiveFunc;
  prepareCompile("", false, primitiveFunc, "");
}
/*
 * 実名ワードヘッダを準備する。
 */
void  prepareNamedCompile(const char *name){
  D_WORD_OBJECT_PRIMITIVE primitiveFunc = searchDictionaryWithError(CW_PUSHIP)->object.primitiveFunc;
  prepareCompile(name, false, primitiveFunc, "");
}
/*
 * プリミティブワードをひとつ、辞書に登録する。
 * 起動時にこれをPRIMITIVE_WORD_INFOの数だけ実行して、最後にprepareUnnamedCompile();している。
 */
void  setupPrimitiveWord(PRIMITIVE_WORD_INFO *wInfo){
  prepareCompile(wInfo->dWordName, wInfo->immediateFlag, wInfo->primitiveFunc, wInfo->dWordComment);
  // 通常のコンパイルの場合は名前付き、名前なしにかかわらずここに中間コードの生成がある。
  finishCompile();
  // finishCompile()は制御構文のアンバランスでfalseを返すが、プリミティブワードの登録時は見なくてもいい。
}
/*
 * STACK_ITEMをコンパイルする。
 */
void  compileStackItem(STACK_ITEM data){
  STACK_ITEM  *ptr = (STACK_ITEM *)(dictionary.nextObjectPtr);
  *(ptr++) = data;
  dictionary.nextObjectPtr = (OBJECT *)ptr;
}
/*
 * OBJECTをコンパイルする。
 */
void  compileObject(OBJECT data){
  *(dictionary.nextObjectPtr++) = data;
}
/* 
 *  通常のワードをコンパイルする。
 */
void  compileStandardWord(D_WORD_HEADER *dWordHeader){
  OBJECT  o;
  o.ptr = (void *)&dWordHeader->object;
  compileObject(o);
}
/*
 * 整数をコンパイルする。
 */
void  compileInt(int data){
  OBJECT  o;
  o.ptr = (void *)&searchDictionaryWithError(CW_PUSHINT)->object;
  compileObject(o);
  o.num = data;
  compileObject(o);
}
/*
 * 浮動小数点数値をコンパイルする。なぜか整数のコンパイルと同じ記述では期待する動作をしない。
 * この時点（2019/12/04）ではコンパイラのバグとしか考えられない。丸一日潰した。orz
 */
void  compileFloat(float data){
  OBJECT  o;
  o.ptr = (void *)&searchDictionaryWithError(CW_PUSHFLOAT)->object;
  compileObject(o);
  o.flt = data;
  compileObject(o);
}
/*
 * 変数をコンパイルする。この定義は標準FORTHに従わない手法を使っている。
 * 通常のワード定義(コンパイル)中に<VAR>を記述すると、$$VARをオブジェクト生成した後sizeof(STACK_ITEM) * 2byteの
 * スペースを生成する。
 * 
 * $$VARは実行時に、そのスペースの先頭アドレスをスタックに積む。ここまでは標準FORTHの変数とおなじことをしているように見える。
 * 
 * しかし構成は単なるワード定義であるので、その後も ';' までに自由に処理を書くことができる。従来のFORTHの変数定義では
 * 不可能だったことが可能になる。たとえば<VAR>に続いてメモリ確保ワードと少しの処理を書けば、ARRAYも定義できる。
 * 工夫と発想次第。
 * 
 * なお、sizeof(STACK_ITEM) * 2byteのスペースの初期値はアトリビュートを除き、オール0であることを保証する。
 */
void  compileVar(){
  STACK_ITEM  o;
  o.attr = ATTR_INTNUM;
  o.value.num = 0;
  compileStandardWord(searchDictionaryWithError(CW_VAR));
  compileStackItem(o);
  compileStackItem(o);     // これは予備のエリア。
}
/*
 * 文字列から整数もしくは浮動小数への変換と辞書内コンパイル中コードへの展開。
 * 変換できるのは10進整数、0xnnn..形式の16進整数、0bnnnn..形式の2進整数、
 * 浮動小数点数、およびssss:oooo形式の16進整数である。(この順に評価)。
 */
boolean compileNumber(const char *name){
  char *resP;
  int   resI = strtol(name, &resP, 0);
  if (*resP == '\0'){
    compileInt(resI);     // 整数化に成功した場合。
    return  true;
  }
  if ((strncmp(name, "0B", 2) == 0)||(strncmp(name, "0b", 2) == 0)){
    resI = strtol(name + 2, &resP, 2);
    if (*resP == '\0'){
      compileInt(resI);     // 整数化に成功した場合。
      return  true;
    }
  }
  float resF = strtod(name, &resP);
  if (*resP == '\0'){
    compileFloat(resF); // 浮動小数点数値化に成功した場合。
    return  true;
  }
  if (strlen(name) == 9){   // ssss:oooo 形式を 0xssssooooと読み替えて数値化できるようにした。
    String str = String(name);
    if (str.indexOf(":") == 4){
      str = String("0x") + str.substring(0, 4) + str.substring(5);
      int   resI = strtol(str.c_str(), &resP, 0);
      if (*resP == '\0'){
        compileInt(resI);     // 整数化に成功した場合。
        return  true;
      }
    }
  }
  return  false;
}
/*
 * 文字列を辞書内に展開する。
 */
boolean compileString(const char *name){
  int len = strlen(name);
  if ((*name != '"') || (*(name + len - 1) != '"'))
    return  false;          // 文字列は両端が'"'である。
  name++;   len -= 2;
  compileStandardWord(searchDictionaryWithError(CW_PUSHSTRING));
  char  *ptr = (char *)dictionary.nextObjectPtr;
  while(len){
    *ptr++ = *name++;
    len--;
  }
  *ptr++ = '\0';
  dictionary.nextObjectPtr = (OBJECT *)ptr;
  return  true;  
}
/*
 * DO,LOOP命令の実行時ルーチンをコンパイルする。
 */
void  compileDo(){
  compileStandardWord(searchDictionaryWithError(CW_DO));
}
void  compileLoop(){
  compileStandardWord(searchDictionaryWithError(CW_LOOP));
}
void  compilePLoop(){
  compileStandardWord(searchDictionaryWithError(CW_PLOOP));
}
/*
 * IF, -IF, ELSE, THEN命令の実行時ルーチンをコンパイルする。
 */
void  compileJump(){
  compileStandardWord(searchDictionaryWithError(CW_JUMP));
}
void  compileIfJump(){
  compileStandardWord(searchDictionaryWithError(CW_IFJUMP));
}
void  compileFIfJump(){
  compileStandardWord(searchDictionaryWithError(CW_FIFJUMP));
}
/*
 * ワードの末尾アドレス+1を返す。このアドレスがつぎのワードの先頭である。
 */
D_WORD_HEADER *getNextWordHeader(D_WORD_HEADER *dWordHeader){
  const char  *namePtr = dWordHeader->dWordName;
  char  len = strlen(namePtr);
  return  (D_WORD_HEADER *)(namePtr + len + 1);
}
/*
 * コンパイラがつぎにオブジェクトを書き込む、定義中のワードのアドレス。
 */
OBJECT        *getNextObjectPtr(){
  return  dictionary.nextObjectPtr;
}
/*
 * 通常の辞書検索。forthEntryから呼び出すのは再帰対応のsearchDictionaryIncludeMe()の方。
 * KZ80FORTHと同様に、大文字と小文字は区別しない。
 */
D_WORD_HEADER *searchDictionary(const char *name){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  dWordHeader = dWordHeader->dPrevWordHeader;
  while ((stricmp(name, dWordHeader->dWordName)) != 0){
    if (dWordHeader->dPrevWordHeader == 0L)
      return  NULL;
    dWordHeader = dWordHeader->dPrevWordHeader;
  }
  return  dWordHeader;
}
/*
 * 辞書を検索する。再帰呼び出しに対応するため、実名ワードヘッダに関しては定義中のワードも検索対象とする。
 * 
 * この機能は isNamedHeader(dWordHeader) を使用するため、dWordHeader->dWordName に
 * ワード名文字列へのポインタが正しくセットされていることを期待している。
 */
D_WORD_HEADER *searchDictionaryIncludeMe(const char *name){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  if (isNamedHeader(dWordHeader) == true){   // 再帰呼び出しのため、名前付き定義中のワードも検索対象になる。
    if((stricmp(name, dWordHeader->dWordName)) == 0)
      return  dWordHeader;
  }
  return  searchDictionary(name);
}
/*
 * 最新語から引数で渡されたdWordHeaderを持つワードまでを辞書から削除する。
 * 単なるupdateNewestWordPtr()にしていないのは、malloc/reallocで確保した
 * usageコメントをfreeするため。
 */
void  forgetDictionary(D_WORD_HEADER *dWordHeaderForget){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  dWordHeader = dWordHeader->dPrevWordHeader;
  while (dWordHeader != dWordHeaderForget){
    if (dWordHeader->dPrevWordHeader == 0L)
      return;     // searchDictionary()でワード名を発見した場合のみここに来るので、これはあり得ない。
    if (dWordHeader->dWordComment != NULL)
      free(dWordHeader->dWordComment);
    dWordHeader = dWordHeader->dPrevWordHeader;
  }
  updateNewestWordPtr(dWordHeader);
}
/*
 * 辞書をダンプする。先頭が"$$"のワード名はシステム用なので表示とワード数カウントをしない。
 * 引数searchSubがNULLでなければ部分文字列の先頭アドレスと解釈し、それを含むワード名のみを表示する。
 */
unsigned int    dumpDictionary(const char *searchSub){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  dWordHeader = dWordHeader->dPrevWordHeader;
  unsigned int   wLen = 0;
  unsigned int   wCount = 0;
  while (true){
    if (strlen(dWordHeader->dWordName) > wLen){
      wLen = strlen(dWordHeader->dWordName);
    }
    if ((dWordHeader = dWordHeader->dPrevWordHeader) == NULL)
      break;
  }
  wLen += 2;
  dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  dWordHeader = dWordHeader->dPrevWordHeader;
  while (true){
    if (strncmp(dWordHeader->dWordName, "$$", 2) != 0){
      if (strstr(dWordHeader->dWordName, searchSub) != NULL){
        wCount++;
        HOST_PORT.print(dWordHeader->dWordName);
        if (dWordHeader->dWordComment != NULL){
          if (*(dWordHeader-> dWordComment) != '\0'){
            for (unsigned int i = strlen(dWordHeader->dWordName); i < wLen; i++){
              HOST_PORT.print(" ");
            }
            HOST_PORT.print(dWordHeader->dWordComment);      
          }
        }
        HOST_PORT.println("");
      }
    }
    if ((dWordHeader = dWordHeader->dPrevWordHeader) == NULL)
      break;
  }
  return  wCount;
}
/*
 * 該当ワードのUsageをアップデートする。
 */
boolean       updateUsage(const char *wordName, const char *newComment){
  D_WORD_HEADER *dWordHeader = searchDictionary(wordName);
  if (dWordHeader == NULL){
    return  false;
  }
  char  *ptr = (char *)realloc(dWordHeader->dWordComment, strlen(newComment) + 1);
  if (ptr == NULL){
    return  false;
  }
  dWordHeader->dWordComment = ptr;
  strcpy(dWordHeader->dWordComment, newComment);
  return  true;
}
/*
 * デバッグ用に定義したワードの内容をダンプする強力な(笑)機能。
 */
void  dumpWord(D_WORD_HEADER *dWordHeader){
  HOST_PORT.print(pack32Bit((unsigned long)dWordHeader));
  HOST_PORT.print(" to ");
  const char *nextWordHeader = (const char *)getNextWordHeader(dWordHeader);
  HOST_PORT.print(pack32Bit((unsigned long)nextWordHeader - 1));
  HOST_PORT.print(" - [");
  HOST_PORT.print(dWordHeader->dWordName);
  HOST_PORT.println("]");
  HOST_PORT.print("prevWord: ");
  HOST_PORT.print(pack32Bit((unsigned long)dWordHeader->dPrevWordHeader));
  HOST_PORT.print(", immediate: ");
  HOST_PORT.print(dWordHeader->immediateFlag == true?"true":"false");
  HOST_PORT.print(", namePtr: ");
  HOST_PORT.print(pack32Bit((unsigned long)dWordHeader->dWordName));
  HOST_PORT.print(", primitiveWord: ");
  HOST_PORT.println(pack32Bit((unsigned long)dWordHeader->object.primitiveFunc));
  HOST_PORT.print("object dump at: ");
  HOST_PORT.print(pack32Bit((unsigned long)&dWordHeader->object.dObject));
  HOST_PORT.print(" - ");
  int   cr = 0;
  for(char *i = (char *)&dWordHeader->object.dObject; i < nextWordHeader; i++){
    HOST_PORT.print(pack8Bit(*i));
    HOST_PORT.print(" ");
    if (++cr == 16){
      HOST_PORT.print("\n                            ");
      cr = 0;
    }
  }
  HOST_PORT.println("");
}
/*
 * コンパイル中のワードにまだひとつもワードがコンパイルされていない場合に真を返す。
 */
boolean   isNewestWordEmpty(){
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;
  return  &dWordHeader->object.dObject[0] == (D_WORD_OBJECT **)getNextObjectPtr();
}
/*
 * ワード名をこれまで生成してきたオブジェクトの末尾にコピーしてコンパイルを完了する。
 * 実名ワードヘッダの場合は各ポインタを更新して、コンパイル結果を辞書に登録する。
 * 無名ワードヘッダの場合はコンパイル結果をフラッシュ（実行 + 削除）する。
 */
boolean finishCompile(){
  int lv = getSStackLevel();
  if (lv > 0){
    STACK_ITEM    o = popSStack();
    switch (o.attr){
      case    ATTR_SYNTAX_DO:
            HOST_PORT.println("'DO' Without 'LOOP'");
            break;
      case    ATTR_SYNTAX_IF:
            HOST_PORT.println("'IF' Without 'THEN'");
            break;
      case    ATTR_SYNTAX_BEGIN:
            HOST_PORT.println("'BEGIN' Without 'UNTIL' or 'REPEAT'");
            break;
      default:
            HOST_PORT.println("Syntax stack unbalance(Unknown)");
    }
    initAllStacks();
    prepareUnnamedCompile();
    return false;
  }
  D_WORD_HEADER *dWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr;  // いま生成中のワードの先頭アドレスを取得する。
  char  *namePtr = (char *)dictionary.nextObjectPtr;      // コンバイルずみオブジェクトの末尾アドレスを取得して
  strcpy(namePtr, dWordHeader->dWordName);                // そこにワード名をセットする。名前なしでも'\0'だけはコピーされる。
  dWordHeader->dWordName = namePtr;                       // セットしたアドレスを記録する。
  //
  if ((_approvalDump == true)&&((isNewestWordEmpty() == false)||(isNamedHeader(dWordHeader) == true))){
    HOST_PORT.print("Compiled: ");
    dumpWord(dWordHeader);
    HOST_PORT.println("");
  }
  //
  if (isNamedHeader(dWordHeader) == true){                // 名前付き(通常のFORTHの)コンパイルなので辞書に登録する。
    dWordHeader = (D_WORD_HEADER *)(namePtr + strlen(namePtr) + 1); // 新しい辞書の先頭を指す。
    dWordHeader->dPrevWordHeader = (D_WORD_HEADER *)dictionary.newestWordPtr; // 直前のワードへのリンクを完成させて、
    updateNewestWordPtr(dWordHeader);     // これで辞書への登録が完了した。
  }else{
    if ((_approvalRUN == true)&&(isNewestWordEmpty() == false)){
      FORTH_RUN(&dWordHeader->object);
    }
//    prepareUnnamedCompile();はforthEntry()に戻ったときに実行されるので、ここでは不要。
  }
  return  true;
}
/*
 * 辞書検索でNULLが返ってきたときにエラーを出す。呼び出し側では必ずNULLチェックをすること。
 */
D_WORD_HEADER *searchDictionaryWithError(const char *name){
  D_WORD_HEADER *res = searchDictionary(name);
  if (res == NULL){
    HOST_PORT.print("Not found in dictionary - ");
    HOST_PORT.println(name);
  }
  return  res;
}
int   dictionaryFree(){
  return  &dictionary.endp - (int *)dictionary.nextObjectPtr;
}
