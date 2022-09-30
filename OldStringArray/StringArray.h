#ifndef __STRINGARRAY__
#define __STRINGARRAY__

class stringArray{
public:
  stringArray();
  ~stringArray();
  void        init(void);
  void        clear(void);
  boolean     append(String);
  String      *peek(int);
  String      read(int);
  String      add(int, String);
  String      add(int, char);
  int         count();
  void        dump(void);     // デバッグ用
private:
  boolean   expandBase(int);
  const int baseStep = 6;     // baseがいっぱいになったときに拡張する要素数。
  String    **base = NULL;    // baseの実体。クラスへのポインタの配列。
  int  baseCount = 0;         // 可変長配列baseに確保されている要素数。この要素数いっぱいに要素があるとは限らない。
  int  dataCount = 0;         // baseに実際に格納されている要素数。0は1行目を表す。dataCount < baseCount
};

#endif
