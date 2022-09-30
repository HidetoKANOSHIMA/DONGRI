#ifndef __MUTABLEARRAY__
#define __MUTABLEARRAY__
/*
 * 文字列（Stringクラス)の可変長配列クラスを定義したので、汎用化を狙ってテンプレートを作成した。
 * 
 * しかしintやfloatなどの単純な型とクラスとでは扱いにかなり差があるため、単純な型専用のテンプレートにした。
 * intとfloatでのテストはしてあるが、むやみに信じないこと。
 */
template <typename T>
class mutableArray{
public:
  mutableArray();
  mutableArray(const mutableArray &obj);
  mutableArray &operator=(const mutableArray<T> &obj);
  ~mutableArray();
  void      init(void);
  void      clear(void);
  boolean   append(T);
  boolean   replace(int, T);
  T         *peek(int);
  T         *peekLast(void);
  int       count();
  void      dump(void);             // デバッグ用（表示は関数内を書き換えて個々のシステムに合わせること）
private:
  boolean   expandBase(int);
  const int baseStep = 6;           // baseがいっぱいになったときに拡張する要素数。
  T         *base = NULL;           // baseの実体。Tの配列。
  const int unitSize = sizeof(T);   // Tのサイズ（sizeof<T>）
  int       baseCount = 0;          // 可変長配列baseに確保されている要素数。この要素数いっぱいに要素があるとは限らない。
  int       dataCount = 0;          // baseに実際に格納されている要素数。0は1行目を表す。dataCount < baseCount
};
/*
 * 定義。テンプレートなので、ソースコード上でTを与えることで定義が実体になる。
 */
template <typename T>     // デフォルトコンストラクタ
mutableArray<T>::mutableArray(){
  init();
}
template <typename T>     // コピーコンストラクタ
mutableArray<T>::mutableArray(const mutableArray<T> &obj){
  baseCount = obj.baseCount;
  dataCount = obj.dataCount;
  base = (T *) malloc(unitSize * baseCount);
  for (int i = 0; i < dataCount; i++)
    base[i] = 0;
}
template <typename T>     // 代入演算子のオーバライド
mutableArray<T> &mutableArray<T>::operator=(const mutableArray<T> &obj){
  clear();
  free(base);
  baseCount = obj.baseCount;
  dataCount = obj.dataCount;
  base = (T *) malloc(unitSize * baseCount);
  for (int i = 0; i < dataCount; i++)
    base[i] = obj.base[i];
  return  (*this);
}
/*
 * malloc. reallocで確保していた動的メモリを解放する。
 */
template <typename T>
mutableArray<T>::~mutableArray(){
  clear();
  free(base);
}
/*
 * 内部で使用する変数類の初期化。コンストラクタから呼ばれる。
 */
template <typename T>
void    mutableArray<T>::init(void){
  base = NULL;
  baseCount = 0;
  dataCount = 0;
}
/*
 * 単純な数値以外は扱わないので、データカウントをクリアするだけ。
 * baseを伸びたままにしているのは頻繁にreallocが発生するのを嫌ったためだが、気にしすぎかも知れない。
 */
template <typename T>
void    mutableArray<T>::clear(void){
  dataCount = 0;
}
/*
 * 配列末尾にdataを追加する。
 * もし追加前にbaseがいっぱいになっていたらbaseエリアを拡張する。
 */
template <typename T>
boolean mutableArray<T>::append(T data){
  if (dataCount == baseCount){
    if (expandBase(baseStep) == false)
      return  false;    
  }
  base[dataCount++] = data;
  return  true;
}
/*
 * baseの[idx]位置を更新する。
 * 保持している最大要素を超えて指定された場合は配列要素を拡張する。
 */
template <typename T>
boolean mutableArray<T>::replace(int idx, T data){
  while (dataCount <= idx){
    if (append(0) == false)
      return  NULL;
  }
  base[idx] = data;
  return  true;
}
/*
 * 任意要素のポインタを返す。
 * 保持している最大要素を超えて指定された場合は配列要素を拡張する。
 */
template <typename T>
T *mutableArray<T>::peek(int idx){
  while (dataCount <= idx){
    if (append(0) == false)
      return  NULL;
  }
  return  &base[idx];
}
/*
 * 末尾(最新)要素のポインタを返す。
 * 保持している最大要素を超えて指定された場合は配列要素を拡張する。
 */
template <typename T>
T *mutableArray<T>::peekLast(void){
  if (dataCount == 0)
    return  NULL;
  return  peek(dataCount - 1);
}
/*
 * 保持しているデータの数を返す。
 */
template <typename T>
int     mutableArray<T>::count(){
  return  dataCount;
}
/*
 * このクラスのキモ。確保した配列用メモリが足りなくなると拡張する。
 * いったん拡張したメモリはclearでも縮めずにそのまま使っている。
 */
template <typename T>
boolean mutableArray<T>::expandBase(int step){
  char  *ptr = (char *)realloc((char *)base, ((baseCount + step) * unitSize));
  if (ptr == NULL)
    return  false;
  if (base != (T*)ptr)
    base = (T*)ptr;
  for ( int i = baseCount; i < (baseCount + step); i++)
    base[i] = 0;
  baseCount += step;
  return  true;
}
/*
 * デバッグ用に保持している全データをダンプする。
 */
template <typename T>
void    mutableArray<T>::dump(void){
  for (int i = 0; i < dataCount; i++){
    Serial.println(base[i]);          // ここのシステムに合わせて書き換えること。
  }
}

#endif
