#include    <arduino.h>
#include    "StringArray.h"
#include    "StreamIO.h"
/*
 * mutableArrayの簡易版。これがないのでパース後の文字列配列を固定長で作るしかなく、
 * さすがにみっともないのでやっつけで作ってしまった。それなりに動いている。
 * テンプレートが使えたら応用が利くんだろうけど、Arduino言語での使い方を見つけられなかった。
 */
stringArray::stringArray(){
  init();
}
/*
 * デストラクタではclearで動的生成した個々のStringのdeleteを実行し、freeで配列用に
 * reallocで確保していた動的メモリを解放する。
 */
stringArray::~stringArray(){
  clear();
  free(base);
}
/*
 * 内部で使用する変数類の初期化。コンストラクタで呼ばれる。
 */
void    stringArray::init(void){
  base = NULL;
  baseCount = 0;
  dataCount = 0;
}
/*
 * 動的生成した個々のStringのインスタンスを破棄する。インスタンスを保持しているbaseを初期化せず
 * のびたまま使っているのは頻繁にreallocが発生するのを嫌ったためだが、気にしすぎかも知れない。
 */
void    stringArray::clear(void){
  if (base != NULL){
    for (int i = dataCount; i > 0; i--){
      int   idx = i - 1;
      if (base[idx] != NULL){
        delete base[idx];
        base[idx] = NULL;
      }
    }
    dataCount = 0;
  }
}
/*
 * 与えられた文字列から新規のStringのインスタンスを作成し、baseの末尾に追加する。
 * もし追加前にbaseがいっぱいになっていたらbaseエリアを拡張する。
 */
boolean stringArray::append(String data){
  if (dataCount == baseCount){
    if (expandBase(baseStep) == false)
      return  false;    
  }
  base[dataCount++] = new String(data);
  return  true;
}
/*
 * 任意行のStringインスタンスポインタを返す。
 * 保持している最大行を超えた行を指定された場合はNULLを返す。
 */
String *stringArray::peek(int line){
  if (line < dataCount)
    return  base[line];
  else
    return  NULL;
}
/*
 * 任意行のStringを返す。
 */
String  stringArray::read(int line){
  String  *sptr = peek(line);
  if (sptr == NULL)
    return  String("");
  else
    return  *(sptr);
}
/*
 * 任意行のStringにStringを追加する。追加後のStringを返す。
 */
String  stringArray::add(int line, String data){
  String  *sptr = peek(line);
  if (sptr == NULL)
    return  String("");
  else{
    *sptr += data;
    return  *(sptr);
  }
}
/*
 * 任意行のStringにcharを追加する。追加後のStringを返す。
 */
String  stringArray::add(int line, char data){
  String  *sptr = peek(line);
  if (sptr == NULL)
    return  String("");
  else{
    *sptr += data;
    return  *(sptr);
  }
}
/*
 * 保持しているデータの行数を返す。
 */
int         stringArray::count(){
  return  dataCount;
}
/*
 * このクラスのキモ。確保した配列用メモリが足りなくなると拡張する。
 * いったん拡張したメモリはclearでも縮めずにそのまま使っている。
 */
boolean stringArray::expandBase(int step){
  char  *ptr = (char *)realloc((char *)base, ((baseCount + step) * sizeof(String *)));
  if (ptr == NULL)
    return  false;
  if (base != (String **)ptr)
    base = (String **)ptr;
  for ( int i = baseCount; i < (baseCount + step); i++)
    base[i] = NULL;
  baseCount += step;
  return  true;
}
/*
 * デバッグ用に保持している全データをダンプする。
 */
void    stringArray::dump(void){
  for (int i = 0; i < dataCount; i++){
    HOST_PORT.println(*base[i]);
  }
}
