#ifndef __MUTABLEARRAYREF__
#define __MUTABLEARRAYREF__

/*
 * Obj-CとかのmutableArrayの簡易版。classのインスタンスをたくさん(笑)追加して貯め込んでいける。
 * 動作前は要素数が決められない配列に使える。ソースコードでmutableArrayRef<String>のように定義して使う。
 * Tはclassであればいいので、mutableArrayRef<mutableArrayRef<String>>みたいなこともできる。実際に
 * DONGRIではParserがマクロの展開に使っている。
 * 
 * Arduinoの標準ライブラリにはこれがないのでパース後の文字列(String)配列を固定長で作るしかなく、
 * まずはそれですませたがさすがにみっともないので、やっつけで作ってしまった。それなりに動いている。
 * テンプレートが使えたら応用が利くんだろうけど、Arduino言語での使い方を見つけられなかった。
 * 
 * …というつもりだったんだが、色々調べてみるとテンプレートは使えることがわかった。
 * それならString専用ではなく汎用化した方が後々使いやすいので、そのように再構成した。
 * 
 * なお、これはクラスのポインタを収納するためのテンプレート。単純な数値のintやfloatの可変長配列は
 * mutableArray.hを使うこと。
 * 
 * 後にこうした用途にはSTLとかいうライブラリ群があることを知ったが、まぁ、作っちゃったので。^^;
 */

template <typename T>
class mutableArrayRef{
public:
  mutableArrayRef();
  mutableArrayRef(const mutableArrayRef &obj);
  mutableArrayRef &operator=(const mutableArrayRef<T> &obj);
  ~mutableArrayRef();
  void        init(void);
  void        clear(void);
  boolean     append(T);
  boolean     replace(T, int);
  T           *peek(int);
  T           *peekLast(void);
  int         count();
private:
  boolean   expandBase(int);
  const int baseStep = 6;           // baseがいっぱいになったときに拡張する要素数。
  const int unitSize = sizeof(T *); // Tのサイズ（sizeof<T *>）
  T         **base = NULL;          // baseの実体。クラスへのポインタの配列。
  int       baseCount = 0;          // 可変長配列baseに確保されている要素数。この要素数いっぱいに要素があるとは限らない。
  int       dataCount = 0;          // baseに実際に格納されている要素数。0は1行目を表す。dataCount < baseCount
};
/*
 * 定義。テンプレートなので、ソースコード上でTを与えることで定義が実体になる。
 */
template <typename T>     // デフォルトコンストラクタ
mutableArrayRef<T>::mutableArrayRef(){
  init();
}
template <typename T>     // コピーコンストラクタ
mutableArrayRef<T>::mutableArrayRef(const mutableArrayRef<T> &obj){
  baseCount = obj.baseCount;
  dataCount = obj.dataCount;
  base = (T **) malloc(unitSize * baseCount);
  for (int i = 0; i < dataCount; i++)
    base[i] = new T(*obj.base[i]);
}
template <typename T>     // 代入演算子のオーバライド
mutableArrayRef<T> &mutableArrayRef<T>::operator=(const mutableArrayRef<T> &obj){
  clear();
  free(base);
  baseCount = obj.baseCount;
  dataCount = obj.dataCount;
  base = (T **) malloc(unitSize * baseCount);
  for (int i = 0; i < dataCount; i++)
    base[i] = new T(*obj.base[i]);
  return  (*this);
}
/*
 * デストラクタでは動的生成した個々のStringのdeleteをclear()実行し、freeで配列用に
 * malloc, reallocで確保していた動的メモリを解放する。
 */
template <typename T>     // デストラクタ
mutableArrayRef<T>::~mutableArrayRef(){
  clear();
  free(base);
}
/*
 * 内部で使用する変数類の初期化。デフォルトコンストラクタから呼ばれる。
 */
template <typename T>
void    mutableArrayRef<T>::init(void){
  base = NULL;
  baseCount = 0;
  dataCount = 0;
}
/*
 * 動的生成した個々のTのインスタンスを破棄する。インスタンスを保持しているbaseを初期化せず
 * 延びたまま使っているのは頻繁にreallocが発生するのを嫌ったためだが、気にしすぎかも知れない。
 */
template <typename T>
void    mutableArrayRef<T>::clear(void){
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
 * 与えられたTから新規のTを作成し、baseの末尾に追加する。
 * もし追加前にbaseがいっぱいになっていたらbaseエリアを拡張する。
 */
template <typename T>
boolean mutableArrayRef<T>::append(T data){
  if (dataCount == baseCount){
    if (expandBase(baseStep) == false)
      return  false;    
  }
  base[dataCount++] = new T(data);
  return  true;
}
/*
 * 与えられたTから新規のTを作成し、base[idx]を更新する。
 */
template <typename T>
boolean mutableArrayRef<T>::replace(T data, int idx){
  if (idx >= dataCount){
      return  false;    
  }
  delete base[idx];
  base[idx] = new T(data);
  return  true;
}
/*
 * 任意要素のTインスタンスポインタを返す。
 * 保持している最大行を超えた行を指定された場合はNULLを返す。
 */
template <typename T>
T *mutableArrayRef<T>::peek(int line){
  if (line < dataCount)
    return  base[line];
  else
    return  NULL;
}
/*
 * 末尾(最新)要素のTインスタンスポインタを返す。
 * 保持している最大行を超えた行を指定された場合はNULLを返す。
 */
template <typename T>
T *mutableArrayRef<T>::peekLast(void){
  if (dataCount == 0)
    return  NULL;
  return  peek(dataCount - 1);
}
/*
 * 保持しているデータの行数を返す。
 */
template <typename T>
int         mutableArrayRef<T>::count(){
  return  dataCount;
}
/*
 * このクラスのキモ。確保した配列用メモリが足りなくなると拡張する。
 * いったん拡張したメモリはclearでも縮めずにそのまま使っている。
 */
template <typename T>
boolean mutableArrayRef<T>::expandBase(int step){
  char  *ptr = (char *)realloc((char *)base, ((baseCount + step) * unitSize));
  if (ptr == NULL)
    return  false;
  if (base != (T **)ptr)
    base = (T **)ptr;
  for ( int i = baseCount; i < (baseCount + step); i++)
    base[i] = NULL;
  baseCount += step;
  return  true;
}

#endif
