#include  <arduino.h>
#include  "System.h"
#include  "StreamIO.h"
#include  "Dictionary.h"
#include  "Primitive.h"
#include  "Utility.h"
/*
 * Compiler and Interpreter
 */
boolean compilerWaitName = false;

/*
 * コンパイラの状態を「これから定義するワード名を待つ名前なしモード」と
 * 「定義中であり、つぎにコンパイルすべきワードを待つ名前付きモード」に切り替える。
 */
void    setCompilerWaitName(){
  compilerWaitName = true;
}
void    resetCompilerWaitName(){
  compilerWaitName = false;
}
boolean isCompilerWaitName(){
  return  compilerWaitName == true;
}
/*
 * コンパイラシステムの初期化。
 * 辞書を初期化し、プリミティブワードを全数登録し、
 * ステートマシンの内部状態を初期化して、無名ワードヘッダを準備する。
 */
void    initSystem(){
  initDictionary();
  initPrimitive();
  resetCompilerWaitName();
  prepareUnnamedCompile();
}
/*
 * スタックとコンパイラのステートマシンを初期化し、無名ワードヘッダをセットする。
 */
void    resetStackAndState(){
  resetCompilerWaitName();
  prepareUnnamedCompile();
  initAllStacks();  
}
/*
 * 定義中の無名ワードヘッダを完結させ、実行する。
 * 制御構文のアンバランスが発生した場合はfinishCompile()がfalseを返してくる。
 */
boolean flushNewestWord(){
  if ((isNewestHeaderIsNamedHeader() == false)&&(isNewestWordEmpty() == false)){  // 無名ワードヘッダのときのみの処理。
    compileStandardWord(searchDictionaryWithError(CW_POPIP));
    if (finishCompile() == false)
      return  false;
    prepareUnnamedCompile();
  }
  return  true;
}

/*
 * 内部状態によって適切なプロンプトを出力する。
 */
void    putPrompt(){
  if (isCompilerWaitName()){
      HOST_PORT.println("name?");   // 名前を待っている状態。
  }else if (isNewestHeaderIsNamedHeader()){
  //    HOST_PORT.println(">>");      // コンパイル中でつぎのワードを待っている状態。
  }else{
      HOST_PORT.print("Ready");     // 即実行（インタプリタ）モード。
      if (getDStackLevel()){
        HOST_PORT.print("("); HOST_PORT.print(getDStackLevel()); HOST_PORT.print(")");
      }
      if (getRStackLevel()){
        HOST_PORT.print(" R:"); HOST_PORT.print(getRStackLevel());
      }
      if (getSStackLevel()){
        HOST_PORT.print(" S:"); HOST_PORT.print(getSStackLevel());
      }
      HOST_PORT.println("");
  }
}
/*
 * 実行時インタプリタ。
 */
boolean FORTH_RUN(D_WORD_OBJECT_FORTH p){
  D_WORD_OBJECT_FORTH runBase[2];
  runBase[0] = p;
  runBase[1] = NULL;
  FORTH_IP ip = runBase;
  while(*ip != NULL){
    D_WORD_OBJECT_PRIMITIVE MY_IP = ((*ip++)->primitiveFunc);
    FORTH_IP RET_IP = ip;
    if ((ip = (FORTH_IP)(*MY_IP)(RET_IP)) == NULL)
      return  false;
  }
  return  true;
}
/*
 * TeensyFORTHのエントリ。最上位のloop()からワード名をひとつずつ渡される。
 */
boolean forthEntry(const char *name){
  if (name == NULL){                          // 空行が来た場合。
    return  flushNewestWord();                // それまでにコンパイルしたオブジェクトを実行する。
  }
  if (isCompilerWaitName() == true){          // ワード定義中でワード名を待っている状態のときは、
    flushNewestWord();                        // まずそれ以前に無名コンパイルされたオブジェクトを実行し、
    resetCompilerWaitName();                  // ワード名待ちの状態をリセットして、
    prepareNamedCompile(name);                // 実名コンパイル状態にする。
    return  true;
  }
  D_WORD_HEADER * dWordHeader = searchDictionaryIncludeMe(name);
  // まずは辞書検索。有名な例題Factのような再帰(自分自身を呼び出す)処理に対応するため、
  // 実名(Named)コンパイル中は自分自身の名前も検索対象にする。
  if (dWordHeader == NULL){   // 辞書になかったので数値化あるいは文字列化を試みる。
    if (compileNumber(name) == false){    // 数値としてのコンパイルに失敗した場合、
      if (compileString(name) == false){  // 文字列としてのコンパイルにも失敗した場合、
        HOST_PORT.print("?? - ");         // エラーとして処理する。
        HOST_PORT.println(name);
        resetStackAndState();
        return  false;
      }
    }
  }else{                      // 辞書にあった場合。
    if (dWordHeader->immediateFlag == true){
      return  FORTH_RUN(&dWordHeader->object);  // Immediate（即実行語）属性の場合は実行する。
    }else{
      compileStandardWord(dWordHeader); // それ以外は通常のワードとしてコンパイルする。
    }
  }
  return  true;
}
