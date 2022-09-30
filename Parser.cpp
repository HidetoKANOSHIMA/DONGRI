#include    <arduino.h>
#include    "StreamIO.h"
#include    "mutableArray.h"
#include    "mutableArrayRef.h"
#include    "Parser.h"

#define     commentMark   String("//")
#define     aliasMark     String("#alias")
#define     echoMark      String("#echo")
#define     stringSpace   String(" ")

/*
 * パース済みの単語を格納する自動拡張型配列。
 * 
 * なんでだかわからんが、classテンプレートをclassのメンバ変数にできないのでここに残してしまった。
 */
  mutableArrayRef<String>   wordPool;
/*
 * ワード名の読み替えのための自動拡張配列。#alias 2DUP DUP DUP // 2DUPをDUP DUPに読み替える。
 * 
 * なんでだかわからんが、classテンプレートをclassのメンバ変数にできないのでここに残してしまった。
 */
  mutableArrayRef<mutableArrayRef<String>>   aliasPool;
/*
 * コンストラクタとデストラクタ。
 */
Parser::Parser(){
  init();
}
Parser::~Parser(){
/*  
 *   echoモードの設定と検査。
 */
}
boolean Parser::isInputEcho(){
  return  echoMode & 1;
}
boolean Parser::isMacroEcho(){
  return  echoMode & 2;
}
void    Parser::setEchoMode(String s){
  echoMode = (s.c_str()[0] - '0') & 3;
}
/*
 * #aliasを定義中の場合にtrueを返す。
 */
boolean Parser::isAliasCommandLine(){
  if (wordPool.count() < 1)
    return  false;
  return  aliasMark.equalsIgnoreCase(*wordPool.peek(0));
}
/*
 * #echoを定義中の場合にtrueを返す。
 */
boolean Parser::isEchoCommandLine(){
  if (wordPool.count() < 1)
    return  false;
  return  echoMark.equalsIgnoreCase(*wordPool.peek(0));
}
/*
 * 文字解析の下請け関数。
 */
char      Parser::lastCharOfString(String org){
    return  org.charAt(org.length() - 1);
}
String    Parser::removelastChar(String org){
  return  org.substring(0, org.length() - 1);
}
/*
 * 20H以下とカンマをデリミタとして扱う。ワード名の区切り判定に用いる。
 * カンマをデリミタに加えたのは、(~, ~)を自然に扱うため。
 */
boolean   Parser::isDelimiter(char c){
  return  ((c <= ' ') || (c == ','));
}
/*
 * パース済みのワード配列に文字列を追加する。
 * ここでaliasの展開をやっている。
 * ただし先頭がaliasやechoコマンドの場合は、展開は行わない。
 */
void  Parser::addWordPool(String newStr){
  mutableArrayRef<String> *ary;
  boolean rwt = false;
  if (!isAliasCommandLine()&&!isEchoCommandLine()){
    for (int i = 0; i < aliasPool.count(); i++){
      ary = aliasPool.peek(i);
      if (newStr.equalsIgnoreCase(*ary->peek(1))){
        for (int j = 2; j < ary->count(); j++)
          wordPool.append(*ary->peek(j));
        rwt = true;
        break;
      }
    }
  }
  if (rwt == false)
    wordPool.append(newStr);
}
/*
 * cnt番目のパース済みのワードを返す。
 */
String  Parser::getWordPool(int cnt){
//  return  wordPool->read(cnt);
  return  *(wordPool.peek(cnt));
}
/*
 * パース済みのワード数を返す。
 */
int     Parser::getWordCount(){
  return  wordPool.count();
}
/*
 * Stringの先頭から連続するデリミタを削除する。
 */
String    Parser::dropDelimiter(String org){
  while(org.length() > 0){
    char  c = org.charAt(0);
    if (isDelimiter(c)){
      org.remove(0, 1);
    } else {
      break;
    }
  }
  return  org;
}
/*
 * バッククォート文字を変換して返す。想定していないシーケンスの場合は文字をそのまま返す。
 */
char    Parser::quotedCharacter(char c){
  switch  (c){
    case  'r':  c = '\r'; break;
    case  'n':  c = '\n'; break;
    case  't':  c = '\t'; break;
    case  'b':  c = '\b'; break;
    case  '0':  c = '\0'; break;
    default:    break;
  }
  return  c;
}
/*
 * <name>(<arg0>,<arg1>)の形式を、arg0 arg1 name に変換する。
 * nameの直後が'('でない場合は通常のFORTH構文として、上記の処理を行わない。
 * これは見づらいと不評のFORTHの構文を見やすくしてみようという試みである。
 */
String    Parser::parserBody(String org){
  String  strNew = "";
  boolean inString = false;           // 文字列解析中フラグ。
  org = dropDelimiter(org);
  while (org.length() > 0){
    char  c = org.charAt(0);          // 左端の1文字によって以下のどれかに分岐。
    org.remove(0, 1);                 // 取り出した左端の文字を削除。
    if (inString){                    // 文字列を処理している状態だったら
      if (c == '"'){
        strNew += c;
        addWordPool(strNew);
        strNew = "";
        inString = false;  
      }else if((c == '\\')&&(org.length() > 0)){
        strNew += quotedCharacter(org.charAt(0));
        org.remove(0, 1);
      }else{
        strNew += c;
      }
    }else if ((c == '\'')&&(org.length() > 1)&&(org.charAt(1) == '\'')){
                                      // シングルクォートだったら
      strNew += String((int)org.charAt(0));
      org.remove(0, 2);
    }else if ((c == '\'')&&(org.length() > 2)&&(org.charAt(0) == '\\')&&(org.charAt(2) == '\'')){
                                      // シングルクォートだったら
      strNew += String((int)quotedCharacter(org.charAt(1)));
      org.remove(0, 3);
    }else if (c == '('){              // 左括弧だったら
      org = parserBody(org);          // 自分自身を再帰的に呼び出して戻ってくる。
      if (strNew.length() > 0){       // 左括弧の直前に保留されたワード名があったら、
          addWordPool(strNew);        // それを出力する。
          strNew = "";
      }
    }else if (c == ')'){              // 右括弧なら上位に戻る。横着ぶっこいて括弧のバランスとか見ていない。
      if (strNew.length() > 0){       // 未出力のワード名があれば
        addWordPool(strNew);          // それを吐き出す。
        strNew = "";
      }
      break;
    }else if (isDelimiter(c)){        // ワード名の区切りだったら、
      if (strNew.length() > 0){       // 未出力のワード名があれば
        addWordPool(strNew);          // それを吐き出す。
        strNew = "";
      }
      org = dropDelimiter(org);       // 区切り文字が続く限り削除。
    }else{                            // 上のどれにも該当しなければ通常のワード名。
      if (c == '"'){
        if (strNew == ""){
          inString = true;
        }
      }
      strNew += c;                    // 未出力のワード名に1文字追加。
    }
  }
  return  strNew + org;               // 呼び出し元に戻る。再帰的にも呼ばれていることに注意。
}
/*
 * aliasコマンドを追加。同名の登録があれば更新する。
 */
void    Parser::appendAliasPool(){
  boolean rwt = false;
  String  org = *wordPool.peek(1);
  mutableArrayRef<String> *ary;
  for (int i = 0; i < aliasPool.count(); i++){
    ary = aliasPool.peek(i);
    if (org.equalsIgnoreCase(*ary->peek(1))){
      aliasPool.replace(wordPool, i);
      rwt = true;
      break;
    }
  }
  if (rwt == false)
    aliasPool.append(wordPool);
/*
  for (int i = 0; i < aliasPool.count(); i++){
    ary = aliasPool.peek(i);
    for (int j = 0; j < ary->count(); j++){
      HOST_PORT.print(*(ary->peek(j)));
      HOST_PORT.print(" / ");
    }
    HOST_PORT.println("");
  }
*/
}
/*
 * いわゆるシンタックスシュガーを実現している。pinMode(15, INPUT) --> 15 INPUT pinMode
 * デリミタで区切られた通常のFORTH構文はそのまま出力される。
 * 戻り値としてパースしたワード数を返す。
 * "//"はコメントマークとして、これ以降行末までをワード数にカウントしない。
 * 
 * パース後の文字列の格納に、さすがにメモリを無駄遣いするStringの実体配列ではなく、
 * Stringへのポインタの配列を使うようにしたので、前回のパースで使われたStringのプールをクリアしている。
 */
int    Parser::parserEntry(String org){
  wordPool.clear();
  String  rest = parserBody(org);
  if (rest.length() > 0)
    addWordPool(rest);
  int   cnt = wordPool.count();
  if (isAliasCommandLine()){
    if (wordPool.count() == 1)       // コマンドのみならaliasPoolのクリア。
      aliasPool.clear();
    else
      appendAliasPool();
    cnt = 0;                          // いずれにせよ、この行はなかったことにする。
  }else if (isEchoCommandLine()){
    if (wordPool.count() == 1)
      HOST_PORT.println(echoMode);    // コマンドのみならechoのモードを表示する。
    else
      setEchoMode(*wordPool.peek(1));
    cnt = 0;                          // いずれにせよ、この行はなかったことにする。
  }else{
    for (int i = 0; i < wordPool.count(); i++){
      if (wordPool.peek(i)->substring(0, 2) == commentMark){
        cnt = i;
        break;
      }
    }
  }
  return  cnt;
}
/*
 * echoモードを反映させるために主ロジックの上位に定義した。
 */
int    Parser::parser(String org){
  org = org.trim();         // 行頭の空白だけではなく、行末の改行コードも省かれる。
  if (isInputEcho()){
    HOST_PORT.println(org);
  }
  int cnt = parserEntry(org);
  if (isMacroEcho() && (cnt > 0)){
    for (int i = 0; i < wordPool.count(); i++){
      HOST_PORT.print(*wordPool.peek(i));
      HOST_PORT.print(" ");
    }
    HOST_PORT.println();
  }
  return  cnt;
}
/*
 * パーサの初期化。なにもしていない。
 * 本来はFORTHシステム全体の初期化initSystem()で行うことだが、コンパイラの内部とは
 * 無関係なソース「テキスト」段階での前処理なので独立させた。
 */
void  Parser::init(){
//  wordPool = new mutableArrayRef<String>;
//  aliasPool = new mutableArrayRef<mutableArrayRef<String>>;
}
