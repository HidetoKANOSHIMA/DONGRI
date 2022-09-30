#include    <arduino.h>
#include    <OneWire.h>
#include    "System.h"
#include    "StreamIO.h"
#include    "Dictionary.h"
#include    "Primitive.h"
#include    "Utility.h"
#include    "mutableArray.h"
#include    "mutableArrayRef.h"
/*
 * 文字列操作のために*Stringの可変長配列を管理するクラスのインスタンス。
 * malloc, realloc, freeを使った自己管理にもトライしたが、
 * ワードのFORGET時にメモリを解放するのがひどく厄介で諦めた。
 */
//stringArray   stringPool;
mutableArrayRef<String>               stringPool;
/*
 * intとbyteの可変長配列クラスを可変長配列として管理するクラスのインスタンス。
 * これも文字列と同様にFORGET時の解放の複雑さを避けるために実装した。
 * 副産物としてARRAYはサイズを指定せずに、何個でも生産できることになった。
 */
mutableArrayRef<mutableArray<int>>    intArrayPool;
mutableArrayRef<mutableArray<byte>>  byteArrayPool;
/*
 * スタック定義と初期化
 */
STACK         dStack, rStack, sStack;
void      initStack(STACK *stack){
  stack->depth = -1;
}
void    initAllStacks(){
  initStack(&dStack);
  initStack(&rStack);
  initStack(&sStack);
}
/*
 * スタック操作ワードの基本部分。プリミティブワードからこれを呼び出す。
 */
int       getStackLevel(STACK *stack){
  return  stack->depth + 1;
}
int       getDStackLevel(){
  return  getStackLevel(&dStack);
}
int       getRStackLevel(){
  return  getStackLevel(&rStack);
}
int       getSStackLevel(){
  return  getStackLevel(&sStack);
}
void      pushStack(STACK *stack, STACK_ITEM data){
  stack->depth++;
  stack->body[stack->depth] = data;
}
STACK_ITEM  popStack(STACK *stack){
  return  stack->body[stack->depth--];
}
STACK_ITEM  peekStack(STACK *stack){
  return  stack->body[stack->depth];
}
STACK_ITEM  pickStack(STACK *stack, int32_t depth){
  return  stack->body[stack->depth - depth];
}
STACK_ITEM  replaceStack(STACK *stack, int32_t depth, STACK_ITEM data){
  STACK_ITEM  old = stack->body[stack->depth - depth];
  stack->body[stack->depth - depth] = data;
  return  old;
}
STACK_ITEM  popSStack(){
  return  popStack(&sStack);
}
void      dropStack(STACK *stack){
  stack->depth--;
}
void      swapStack(STACK *stack){
  STACK_ITEM  o1 = popStack(stack);
  STACK_ITEM  o2 = popStack(stack);
  pushStack(stack, o1);
  pushStack(stack, o2);  
}
/*
 * スタックにデータを積むための下請け関数。アトリビュートごとに用意した。
 */
void      pushDStackInt(int data){
  STACK_ITEM  o;
  o.value.num = data;
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
}
void      pushDStackFloat(float data){
  STACK_ITEM  o;
  o.value.flt = data;
  o.attr = ATTR_FLOATNUM;
  pushStack(&dStack, o);
}
void      pushDStackString(const void *data){
  STACK_ITEM  o;
  o.value.num = (int)data;
  o.attr = ATTR_STRING;
  pushStack(&dStack, o);
}
void      pushDStackStringRef(const void *data){
  STACK_ITEM  o;
  o.value.num = (int)data;
  o.attr = ATTR_STRINGREF;
  pushStack(&dStack, o);
}
void      pushDStackPtr(const void *data){
  STACK_ITEM  o;
  o.value.num = (int)data;
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
}
/*
 * dataが浮動小数点数値なら整数値に変換して返す。
 */
STACK_ITEM  toIntNum(STACK_ITEM data){
  if (data.attr == ATTR_FLOATNUM){
    data.value.num = (int)data.value.flt;
  }
  data.attr= ATTR_INTNUM;
  return  data;
}
/*
 * dataが浮動小数点数値でなければ浮動小数点数値に変換して返す。
 */
STACK_ITEM  toFloatNum(STACK_ITEM data){
  if (data.attr != ATTR_FLOATNUM){
    data.value.flt = (float)data.value.num;
    data.attr= ATTR_FLOATNUM;
  }
  return  data;
}
/*
 * dataが浮動小数点数値でなければ属性を文字列にして返す。
 * 元の属性が浮動小数点数ならば変換しないというルールは設けているが、
 * ポインタの指す先が本当に文字列かどうかの検査はしていない。使用には注意が必要。
 */
STACK_ITEM  toString(STACK_ITEM data){
  if (data.attr != ATTR_FLOATNUM){
    data.attr= ATTR_STRING;
  }
  return  data;
}
/*
 * コンスタントをスタックに積むワードの下請けルーチン。
 */
FORTH_IP   pwCONSTANT(FORTH_IP retIP, int constant){
  pushDStackInt(constant);
  return  retIP;
}
/*
 * 主に演算系のワードで使うため、2個のSTACK_ITEMの精度(整数か浮動小数点数か)を精度の高い方に合わせる。
 * 戻り値として変換後のSTACK_ATTRを返す。
 */
STACK_ATTR  sameAttr(STACK_ITEM *o1, STACK_ITEM *o2){
  if (o1->attr == ATTR_FLOATNUM){
    if (o2->attr == ATTR_FLOATNUM){
      return  ATTR_FLOATNUM;
    }else{
      o2->attr = ATTR_FLOATNUM;
      o2->value.flt = (float)o2->value.num;
      return  ATTR_FLOATNUM;
    }
  }else{
    if (o2->attr != ATTR_FLOATNUM){
      return  ATTR_INTNUM;
    }else{
      o1->attr = ATTR_FLOATNUM;
      o1->value.flt = (float)o1->value.num;
      return  ATTR_FLOATNUM;
    }
  }
}
/*
 * lambda式の中でマクロが展開できないケースがあるので、回避策として外側に関数を用意した。
 */
void  lambdaABS(){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(abs(o.value.flt));
  }else{
    pushDStackInt(abs(o.value.num));
  }
}
void  lambdaCONSTRAIN(){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  STACK_ITEM  o3 = popStack(&dStack);
  sameAttr(&o1, &o2);
  sameAttr(&o1, &o3);
  if (sameAttr(&o2, &o3) == ATTR_FLOATNUM){
    pushDStackFloat(constrain(o3.value.flt, o2.value.flt, o1.value.flt));
  }else{
    pushDStackInt(constrain(o3.value.num, o2.value.num, o1.value.num));
  } 
}
void  lambdaMAP(){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  STACK_ITEM  o3 = popStack(&dStack);
  STACK_ITEM  o4 = popStack(&dStack);
  STACK_ITEM  o5 = popStack(&dStack);
  sameAttr(&o1, &o2);
  sameAttr(&o1, &o3);
  sameAttr(&o1, &o4);
  sameAttr(&o1, &o5);
  sameAttr(&o2, &o3);
  sameAttr(&o2, &o4);
  sameAttr(&o2, &o5);
  sameAttr(&o3, &o4);
  sameAttr(&o3, &o5);
  if (sameAttr(&o4, &o5) == ATTR_FLOATNUM){
    pushDStackFloat(map(o5.value.num, o4.value.num, o3.value.flt, o2.value.flt, o1.value.flt));
  }else{
    pushDStackInt(map(o5.value.num, o4.value.num, o3.value.num, o2.value.num, o1.value.num));
  } 
}
/*
 * 配列機能のための下請けルーチン。
 * 
 * DONGRIの配列は通常のFORTHのように生成時に要素数を指定するのではなく、必要に応じて要素数を拡張していく柔軟なもの。
 * このためにmutableArray<T>という拡張可能な配列クラスをテンプレートとして用意し、さらに複数のmutableArray<T>を
 * 使うことが可能なように、mutableArrayRef<mutableArray<T>>というコンテナクラスを用意して管理している。
 * 
 * mutableArray<T>はそのインスタンスポインタを変数にセットして使用する。DONGRIの変数はデータタ属性も管理しているので、
 * 変数の内容がデータ属性mutableArray<T>のインスタンスを表すATTR_ARRAYREFではなかった場合、新しいmutableArray<T>を
 * 生成して変数にセットする。以下はそのための下請けルーチン。Tとしてはintとbyteがある。
 */
mutableArray<int> *setupIntArray(STACK_ITEM o1){
  STACK_ITEM  *varPtr = (STACK_ITEM *)o1.value.ptr;
  if (varPtr->attr != ATTR_ARRAYREF){
    // 変数がATTR_ARRAYREFでなかった場合は新しいARRAYREFを生成して上書きする。
    intArrayPool.append(mutableArray<int>());
    varPtr->attr = ATTR_ARRAYREF;
    varPtr->value.ptr = intArrayPool.peekLast();
    // 生成してpoolに追加したmutableArray<int>へのポインタを<VAR>にセットする。
  }
  return  (mutableArray<int> *)varPtr->value.ptr;
}
mutableArray<byte>  *setupByteArray(STACK_ITEM o1){
  STACK_ITEM  *varPtr = (STACK_ITEM *)o1.value.ptr;
  if (varPtr->attr != ATTR_ARRAYREF){
    // 変数の内容がATTR_ARRAYREFでなかった場合は新しいARRAYREFを生成して上書きする。
    byteArrayPool.append(mutableArray<byte>());
    varPtr->attr = ATTR_ARRAYREF;
    varPtr->value.ptr = byteArrayPool.peekLast();
    // 生成してpoolに追加したmutableArray<byte>へのポインタを<VAR>にセットする。
  }
  return  (mutableArray<byte> *)varPtr->value.ptr;
}
/* =====================================================================
 * 辞書登録のためのプリミティブワードのテーブル。
 * プリフィクスは、PWがプリミティブワード、PWIはプリミティブワードでイミディエイト、 
 * CWはコンパイラが暗黙に生成するコンパイリングワード。
 * 当初は定数テーブルに関数へのポインタを持っていたが、定義と参照が離れてしまって
 * わかりにくくなってきたのでラムダ式で記述し直した。ただし複数のラムダ式から参照
 * される関数は従来どおり外に出してある。無名関数は名前で参照できないので、当たり前だ。
 * 
 * プリミティブワードは、通常はもらったretIP(戻り番地)をそのまま戻り値とする。
 * 
 * 算術演算の精度（整数 or 浮動小数点数）については、単項演算はスタックトップデータの精度に従う。
 * 2項演算における左右項の精度の一致はワード内部で行うためプログラマは気にしなくていい。
 * なお精度変換のために、スタックトップに作用するFLOATおよびINT を用意する。
 * 
 * =====================================================================
 */
PRIMITIVE_WORD_INFO   wordInfo[] = {
  // =================================================================
  // スタック操作系
  // =================================================================
{PW_DUP, false, [](FORTH_IP retIP){
  STACK_ITEM  o = peekStack(&dStack);
  pushStack(&dStack, o);
  return  retIP;}, "(data1 -> data1, data1)"},
{PW_SWAP, false, [](FORTH_IP retIP){
  swapStack(&dStack);
  return  retIP;}, "(data1, data2 -> data2, data1)"},
{PW_DROP, false, [](FORTH_IP retIP){
  dropStack(&dStack);
  return  retIP;}, "(data1 -> .)"},
{PW_PICK, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.value.num = pickStack(&dStack, o.value.num).value.num;
  pushStack(&dStack, o);
  return  retIP;}, "(data1, data2, data3 2 -> data1, data2, data3, data2)"},
{PW_OVER, false, [](FORTH_IP retIP){
  STACK_ITEM  o = pickStack(&dStack, 1);
  pushStack(&dStack, o);
  return  retIP;}, "(data1, data2 -> data1, data2, data1) Same as \"1 PICK\"."},
{PW_QDUP, false, [](FORTH_IP retIP){
  STACK_ITEM  o = peekStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    if (o.value.flt != 0.0f){
      pushStack(&dStack, o);
    }
  }else{
    if (o.value.num != 0){
      pushStack(&dStack, o);
    }      
  }
  return  retIP;}, "(false -> false)(true -> true, true) Duplicate when top is not false."},
{PW_ROLL, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = pickStack(&dStack, o1.value.num);
  int depth = getDStackLevel();
  for (int i = depth - o1.value.num; i < depth; i++){
    dStack.body[i - 1] = dStack.body[i];   
  }
  dStack.body[depth - 1] = o2;
  return  retIP;}, "(data1, data2, data3, data4 2 -> data1, data3, data4, data2) Move to data[n] by rotate."},
{PW_RTOD, false, [](FORTH_IP retIP){
  pushStack(&dStack, popStack(&rStack));
  return  retIP;}, "(. -> RStack top) Puh RStack top to DStack."},
{PW_DTOR, false, [](FORTH_IP retIP){
  pushStack(&rStack, popStack(&dStack));
  return  retIP;}, "(data1 -> .) Push data1 to RStack."},
{PW_RDROP, false, [](FORTH_IP retIP){
  dropStack(&rStack);
  return  retIP;}, "(. -> .) Drop RStack top."},
{PW_DEPTH, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.value.num = getDStackLevel();
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
  return  retIP;}, "(data1, data2 -> data1, data2, 2)"},
  // =================================================================
  // アトリビュートの変換
  // =================================================================
{PW_ATTR_INT, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    o.attr = ATTR_INTNUM;
    o.value.num = (int)o.value.flt;
  }
  pushStack(&dStack, o);
  return  retIP;}, "((float) -> (int)) Reform float to int."},
{PW_ATTR_FLOAT, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr != ATTR_FLOATNUM){
    o.attr = ATTR_FLOATNUM;
    o.value.flt = (float)o.value.num;
  }
  pushStack(&dStack, o);
  return  retIP;}, "((int) -> (float)) Reform int to float."},
{PW_ATTR_STRING, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.attr = ATTR_STRING;
  pushStack(&dStack, o);
  return  retIP;}, "((int) -> (string)) Reform int to float."},
{PW_ATTR_CHAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_INTNUM){
    if ((o.value.num <= 0x20)&&(o.value.num >= 0x7f)){
      o.attr = ATTR_CHAR;
    }
  }else if (o.attr == ATTR_STRING){
    o.value.num = *(char *)o.value.ptr;
    o.attr = ATTR_CHAR;
  }else{
    o.value.num = 0;
    o.attr = ATTR_INTNUM;
  }
  pushStack(&dStack, o);
  return  retIP;}, "((int) -> (char)) Reform int to char."},
  // =================================================================
  // アトリビュートの確認
  // =================================================================
{PW_ATTR_INTQ, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_INTNUM){
    pushDStackInt(true);
  }else{
    pushDStackInt(false);
  }
  return  retIP;}, "(. -> true/false) Stack top is int?."},
{PW_ATTR_FLOATQ, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackInt(true);
  }else{
    pushDStackInt(false);
  }
  return  retIP;}, "(. -> true/false) Stack top is float?."},
{PW_ATTR_STRINGQ, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_STRING){
    pushDStackInt(true);
  }else{
    pushDStackInt(false);
  }
  return  retIP;}, "(. -> true/false) Stack top is string?."},
{PW_ATTR_CHARQ, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_CHAR){
    pushDStackInt(true);
  }else{
    pushDStackInt(false);
  }
  return  retIP;}, "(. -> true/false) Stack top is char?."},
  // =================================================================
  // 算術演算系
  // =================================================================
{PW_ADD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackFloat(o2.value.flt + o1.value.flt);
  else
    pushDStackInt(o2.value.num + o1.value.num);
  return  retIP;}, "(data1, data2 -> data1 + data2)"},
{PW_SUB, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackFloat(o2.value.flt - o1.value.flt);
  else
    pushDStackInt(o2.value.num - o1.value.num);
  return  retIP;}, "(data1, data2 -> data1 - data2)"},
{PW_INC, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(o.value.flt + 1.0f);
  }else{
    pushDStackInt(o.value.num + 1);
  }
  return  retIP;}, "(data1 -> data1 + 1)"},
{PW_DEC, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(o.value.flt - 1.0f);
  }else{
    pushDStackInt(o.value.num - 1);
  }
  return  retIP;}, "(data1 -> data1 - 1)"}, 
{PW_DIV, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackFloat(o2.value.flt / o1.value.flt);
  else
    pushDStackInt(o2.value.num / o1.value.num);
  return  retIP;}, "(data1, data2 -> data1 / data2)"},
{PW_DIVMOD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM){
    pushDStackFloat((int)o2.value.flt % (int)o1.value.flt);
    pushDStackFloat(o2.value.flt / o1.value.flt);
  }else{
    pushDStackInt(o2.value.num % o1.value.num);
    pushDStackInt(o2.value.num / o1.value.num);
  }
  return  retIP;}, "(data1, data2 -> data1 % data2, data1 / data2)"},
{PW_MUL, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackFloat(o2.value.flt * o1.value.flt);
  else
    pushDStackInt(o2.value.num * o1.value.num);
  return  retIP;}, "(data1, data2 -> data1 * data2)"},
{PW_EQUAL, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (o1.value.num == o2.value.num)
    o1.value.num = true;
  else
    o1.value.num = false;
  o1.attr = ATTR_INTNUM;
  pushStack(&dStack, o1);
  return  retIP;}, "(data1, data2 -> true/false) data1 == data2?"},
{PW_NOTEQUAL, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (o1.value.num == o2.value.num)
    o1.value.num = false;
  else
    o1.value.num = true;
  o1.attr = ATTR_INTNUM;
  pushStack(&dStack, o1);
  return  retIP;}, "(data1, data2 -> true/false) data1 != data2?"},
{PW_GREATER, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackInt((o2.value.flt > o1.value.flt)?true:false);
  else
    pushDStackInt((o2.value.num > o1.value.num)?true:false);
  return  retIP;}, "(data1, data2 -> true/false) data1 > data2?"},
{PW_SMALLER, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM)
    pushDStackInt((o2.value.flt < o1.value.flt)?true:false);
  else
    pushDStackInt((o2.value.num < o1.value.num)?true:false);
  return  retIP;}, "(data1, data2 -> true/false) data1 < data2?"},
{PW_EQUALZERO, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM)
    pushDStackInt((o.value.flt == 0.0f)? true : false);
  else
    pushDStackInt((o.value.num == 0)? true : false);    
  return  retIP;}, "(data1 -> true/false) data1 == 0?"},
{PW_GREATERZERO, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM)
    pushDStackInt((o.value.flt > 0.0f)? true : false);
  else
    pushDStackInt((o.value.num > 0)? true : false);    
  return  retIP;}, "(data1 -> true/false) data1 > 0?"},
{PW_SMALLERZERO, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM)
    pushDStackInt((o.value.flt < 0.0f)? true : false);
  else
    pushDStackInt((o.value.num < 0)? true : false);    
  return  retIP;}, "(data1 -> true/false) data1 < 0?"},
  // =================================================================
  // 論理およびビット演算系（ATTR_INTNUMであることを期待している）
  // =================================================================
{PW_AND, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  o1.value.num &= (unsigned long)o2.value.num;
  pushStack(&dStack, o1);
  return  retIP;}, "(data1, data2 -> data1 AND data2)"},
{PW_OR, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  o1.value.num |= o2.value.num;
  pushStack(&dStack, o1);
  return  retIP;}, "(data1, data2 -> data1 OR data2)"},
{PW_XOR, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  o1.value.num ^= o2.value.num;
  pushStack(&dStack, o1);
  return  retIP;}, "(data1, data2 -> data1 XOR data2)"},
{PW_NOT, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.value.num = ~o.value.num;
  pushStack(&dStack, o);
  return  retIP;}, "(data1 -> NOT data1)"}, 
{PW_SHR, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  o2.value.num >>= o1.value.num;
  pushStack(&dStack, o2);
  return  retIP;}, "(data1, data2 -> data1 SHR data2)"},
{PW_SHL, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  o2.value.num <<= o1.value.num;
  pushStack(&dStack, o2);
  return  retIP;}, "(data1, data2 -> data1 SHL data2)"},
  // =================================================================
  // 数学関数（三角関数）系
  // =================================================================
{PW_MIN, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM){
    pushDStackFloat(min(o1.value.flt, o2.value.flt));
  }else{
    pushDStackInt(min(o1.value.num, o2.value.num));
  }
  return  retIP;}, "(data1, data2 -> min)"},
{PW_MAX, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM){
    pushDStackFloat(max(o1.value.flt, o2.value.flt));
  }else{
    pushDStackInt(max(o1.value.num, o2.value.num));
  }
  return  retIP;}, "(data1, data2 -> max)"},
{PW_ABS, false, [](FORTH_IP retIP){
  lambdaABS();
  return  retIP;}, "(data -> abs(data))"},
{PW_CONSTRAIN, false, [](FORTH_IP retIP){
  lambdaCONSTRAIN();
  return  retIP;}, "(x, low, high -> constrain(low .. high)"},
{PW_MAP, false, [](FORTH_IP retIP){
  lambdaMAP();
  return  retIP;}, "(x, fromLow, fromHigh, toLow, toHigh -> map(x))"},
{PW_POW, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (sameAttr(&o1, &o2) == ATTR_FLOATNUM){
    pushDStackFloat(pow(o2.value.flt, o1.value.flt));
  }else{
    pushDStackInt(pow(o2.value.num, o1.value.num));
  }
  return  retIP;}, "(data1, data2 -> pow(data1, data2))"},
{PW_SQRT, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(sqrt(o.value.flt));
  }else{
    pushDStackInt(sqrt(o.value.num));
  }
  return  retIP;}, "(data -> sqrt(data))"},
{PW_SIN, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(sin(o.value.flt));
  }else{
    pushDStackInt(sin(o.value.num));
  }
  return  retIP;}, "(radian -> sin(radian))"},
{PW_COS, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(cos(o.value.flt));
  }else{
    pushDStackInt(cos(o.value.num));
  }
  return  retIP;}, "(radian -> cos(radian))"},
{PW_TAN, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(tan(o.value.flt));
  }else{
    pushDStackInt(tan(o.value.num));
  }
  return  retIP;}, "(radian -> tan(radian))"},
{PW_RADTODEG, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(o.value.flt*(180.0 / PI));
  }else{
    pushDStackInt(o.value.num*(180.0 / PI));
  }
  return  retIP;}, "(radian -> degree)"},
{PW_DEGTORAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM){
    pushDStackFloat(o.value.flt*(180.0 / PI));
  }else{
    pushDStackInt(o.value.num*(PI / 180.0));
  }
  return  retIP;}, "(degree -> radian)"},
  // =================================================================
  // 表示系
  // =================================================================
{PW_PRINT, false, [](FORTH_IP retIP){
    STACK_ITEM  o = popStack(&dStack);
  if (o.attr == ATTR_FLOATNUM)
    HOST_PORT.print(o.value.flt, 6);
  else if (o.attr == ATTR_STRING)
    HOST_PORT.print((char *)o.value.ptr);
  else if (o.attr == ATTR_INTNUM)
    HOST_PORT.print(o.value.num);
  else
    HOST_PORT.print(pack32Bit(o.value.num));
  return  retIP;}, "(data1 -> .) Print data1 by attribute."},
{PW_PRINTCHAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  HOST_PORT.print((char)o.value.num);  
  return  retIP;}, "(data1 -> .) Print data1 by char."},
{PW_PRINTCHARVISIBLE, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  char  oc = (char)o.value.num;
  if ((oc < 0x20)||(oc > 0x7f))
    HOST_PORT.print((char)'.');
  else  
    HOST_PORT.print((char)o.value.num);  
  return  retIP;}, "(data1 -> .) Print data1 only visible char."},
{PW_CRLF, false, [](FORTH_IP retIP){
  HOST_PORT.println("");
  return  retIP;}, "(. -> .) Print CR/LF"},
{PW_H8, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.value.ptr = pack8Bit(o.value.num & 0xff);
  o.attr = ATTR_STRING;
  pushStack(&dStack, o);
  return  retIP;}, "(data1 -> .) Print data1 by HEX 8 digit."},
{PW_H16, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.value.ptr = pack16Bit(o.value.num & 0xffff);
  o.attr = ATTR_STRING;
  pushStack(&dStack, o);
  return  retIP;}, "(data1 -> .) Print data1 by HEX 16 digit."},
{PW_H32, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  o.value.ptr = pack32Bit(o.value.num);
  o.attr = ATTR_STRING;
  pushStack(&dStack, o);
  return  retIP;}, "(data1 -> .) Print data1 by HEX 32 digit."},
  // =================================================================
  // 制御構文などコンパイル中に特殊なコードが生成されるワード群
  // =================================================================
{PW_HALT, false, [](FORTH_IP retIP){
  return  (FORTH_IP)NULL;}, "** Don't touch **"},
/*
 * このプリミティブワードCW_PUSHIPは戻り番地に戻らない。
 * 戻り番地はリターンスタックに積み、つぎの処理番地を自分自身のアドレスの直後(dObject[0])とする。
 * 呼び出し元への復帰FORTH_IPは$$POPIP(cwPopIP)によってリターンスタックから取り出して行う。
 * 
 * struct  PRIMITIVE_WORD_INFO{
 *   const   char            *dWordName;
 *   bool                    immediateFlag;
 *   D_WORD_OBJECT_PRIMITIVE primitiveFunc;
 *   const        char       *dWordComment;
 * };
 * 
 * struct  D_WORD_OBJECT{
 *   D_WORD_OBJECT_PRIMITIVE primitiveFunc;
 *   D_WORD_OBJECT_FORTH     dObject[0];
 * };
 * 
 */ 
{CW_PUSHIP, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.attr = ATTR_INTNUM;
  o.value.ptr = (void *)retIP;          // 戻り番地をリターンスタックに積む。
  pushStack(&rStack, o);
  //
  retIP--;
  FORTH_IP  myIP = (*retIP)->dObject;   // つぎの実行番地を戻り値とする。
  return  myIP;}, "** System use **"},
{CW_POPIP, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&rStack);
  return  (FORTH_IP)o.value.ptr;}, "** System use **"},            
{CW_PUSHINT, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.attr = ATTR_INTNUM;
  o.value.num = (int)*retIP;
  pushStack(&dStack, o);
  return  ++retIP;}, "** System use **"},
{CW_PUSHFLOAT, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.attr = ATTR_FLOATNUM;
  o.value.num = (int)*retIP;
  pushStack(&dStack, o);
  return  ++retIP;}, "** System use **"},
{CW_PUSHSTRING, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  char *ptr = (char *)retIP;
  o.attr = ATTR_STRING;
  o.value.ptr = ptr;
  pushStack(&dStack, o);
  ptr += strlen(ptr) + 1;
  return  (FORTH_IP)ptr;}, "** System use **"},
{PW_HELLO, false, [](FORTH_IP retIP){
  HOST_PORT.println("Hello, I'm TeensyFORTH.");
  return  retIP;}, "** System use **"},
{PWI_DO, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  compileDo();    // cwDo()をコンパイルする。
  o.attr = ATTR_SYNTAX_DO;
  o.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o);
  return  retIP;}, "(Limit Start -> .) 10 0 DO ~ LOOP / 10 0 DO ~ n +LOOP."},
{CW_DO, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // ループカウンタの初期値。
  STACK_ITEM  o2 = popStack(&dStack);   // 終値。
  pushStack(&rStack, o2);
  pushStack(&rStack, o1);
  return  retIP;}, "** System use **"},
{PWI_LOOP, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'LOOP' Without 'DO'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_DO){
      HOST_PORT.println("'LOOP' Without 'DO'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  compileLoop();
  compileObject(o.value);
  return  retIP;}, "(. -> .) Look at 'DO'."},
{CW_LOOP, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&rStack);   // ループカウンタの初期値。
  STACK_ITEM  o2 = popStack(&rStack);   // 終値。
  if (++o1.value.num >= o2.value.num){
    return  ++retIP;
  }else{
    pushStack(&rStack, o2);
    pushStack(&rStack, o1);
    return  (FORTH_IP)*retIP;}
    }, "** System use **"},
{PWI_PLOOP, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'+LOOP' Without 'DO'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_DO){
      HOST_PORT.println("'+LOOP' Without 'DO'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  compilePLoop();
  compileObject(o.value);
  return  retIP;}, "(bias -> .) Look at 'DO'."},
{CW_PLOOP, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&rStack);   // ループカウンタの初期値。
  STACK_ITEM  o2 = popStack(&rStack);   // 終値。
  STACK_ITEM  o3 = popStack(&dStack);   // 増分。
  o1.value.num += o3.value.num;
  if (o1.value.num >= o2.value.num){
    return  ++retIP;
  }else{
    pushStack(&rStack, o2);
    pushStack(&rStack, o1);
    return  (FORTH_IP)*retIP;}
    }, "** System use **"},
{PW_LEAVE, false, [](FORTH_IP retIP){
  replaceStack(&rStack, 0, pickStack(&rStack, 1));
  return  retIP;}, "(. -> .) Force leave this loop at (+)LOOP."},
{PW_RREAD, false, [](FORTH_IP retIP){
  pushStack(&dStack, peekStack(&rStack));
  return  retIP;}, "(. -> RStack top) Same as 'I'."},
{PW_I, false, [](FORTH_IP retIP){
  pushStack(&dStack, peekStack(&rStack));
  return  retIP;}, "(. -> My loop counter)"},
{PW_J, false, [](FORTH_IP retIP){
  pushStack(&dStack, pickStack(&rStack, 2));
  return  retIP;}, "(. -> Outside loop counter)"},
{PWI_IF, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  compileFIfJump();                     // cwIf()をコンパイルする。
  o.attr = ATTR_SYNTAX_IF;
  o.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o);
  compileObject(o.value);               // 仮の値（飛び先）
  return  retIP;}, "(true/false -> .) true/false IF (true path) {ELSE (false path)} THEN."},
{CW_JUMP, false, [](FORTH_IP retIP){
  return  (FORTH_IP)*retIP;}, "** System use **"},          // 無条件で指定飛び先アドレスへ。
{PWI_FIF, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  compileIfJump();                      // cwFIf()をコンパイルする。
  o.attr = ATTR_SYNTAX_IF;
  o.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o);
  compileObject(o.value);               // 仮の値（飛び先）
  return  retIP;}, "(true/false -> .) true/false -IF (false path) {ELSE (true path)} THEN."},
{CW_IFJUMP, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);    // 真偽値。
  if (o.value.num != 0){                // 真ならばTHEN（もしくはELSE）へ。
    return  (FORTH_IP)*retIP;
  }else{
    return  ++retIP;}                   // 偽ならばTHEN（もしくはELSE）までを実行。
  }, "** System use **"},
{PWI_ELSE, true, [](FORTH_IP retIP){
  STACK_ITEM  o1, o2;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'ELSE' Without 'IF' or '-IF'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o1 = popStack(&sStack);
    if (o1.attr != ATTR_SYNTAX_IF){
    HOST_PORT.println("'ELSE' Without 'IF' or '-IF'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  compileJump();                      // cwFIf()をコンパイルする。
  o2.attr = ATTR_SYNTAX_IF;
  o2.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o2);
  compileObject(o2.value);               // 仮の値（飛び先）
  //
  OBJECT  **ptr = (OBJECT **)o1.value.ptr;
  *ptr = getNextObjectPtr();
  return  retIP;}, "(. -> .) Look at IF/-IF."},
{CW_FIFJUMP, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);    // 真偽値。
  if (o.value.num == 0){                // 偽ならばTHEN（もしくはELSE）へ。
    return  (FORTH_IP)*retIP;
  }else{
    return  ++retIP;}                   // 真ならばTHEN（もしくはELSE）までを実行。
  }, "** System use **"},
  
{PWI_THEN, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'THEN' Without 'IF' or '-IF'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_IF){
    HOST_PORT.println("'THEN' Without 'IF' or '-IF'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  OBJECT  **ptr = (OBJECT **)o.value.ptr;
  *ptr = (OBJECT *)getNextObjectPtr();
  return  retIP;}, "(. -> .) Look at IF/-IF."},
  
{PW_EXIT, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&rStack);
  return  (FORTH_IP)o.value.ptr;}, "(. -> .) Exit this word."},
  
{PWI_BEGIN, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.attr = ATTR_SYNTAX_BEGIN;
  o.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o);
  return  retIP;}, "(. -> .) BEGIN true/false UNTIL."},
  
{PWI_UNTIL, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'UNTIL' Without 'BEGIN'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_BEGIN){
      HOST_PORT.println("'UNTIL' Without 'BEGIN'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  compileFIfJump();
  compileObject(o.value);               // 仮の値（飛び先）
  return  retIP;}, ""},
  
{PWI_WHILE, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'WHILE' Without 'BEGIN'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_BEGIN){
      HOST_PORT.println("'WHILE' Without 'BEGIN'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  pushStack(&sStack, o);
  compileFIfJump();
  o.attr = ATTR_SYNTAX_WHILE;
  o.value.ptr = getNextObjectPtr();
  pushStack(&sStack, o);
  compileObject(o.value);               // 仮の値（飛び先）
  return  retIP;}, ""},
  
{PWI_REPEAT, true, [](FORTH_IP retIP){
  STACK_ITEM  o;
  if (getSStackLevel() < 1){
    HOST_PORT.println("'REPEAT' Without 'BEGIN' or 'WHILE'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }else{
    o = popStack(&sStack);
    if (o.attr != ATTR_SYNTAX_WHILE){
      HOST_PORT.println("'REPEAT' Without 'WHILE'");
      initAllStacks();
      return  (FORTH_IP)NULL;
    }
  }
  OBJECT  **ptr = (OBJECT **)o.value.ptr;
  *ptr = getNextObjectPtr() + 2;    // WHILEで偽だった場合の飛び先（REPEATが生成するJump命令の先）を解決。
  o = popStack(&sStack);            // BEGINのシンタックス情報を思い出す。
  if (o.attr != ATTR_SYNTAX_BEGIN){
    HOST_PORT.println("'REPEAT' Without 'BEGIN'");
    initAllStacks();
    return  (FORTH_IP)NULL;
  }
  compileJump();
  compileObject(o.value);               // BEGINへの飛び先を解決。
  return  retIP;}, ""},
  // =================================================================
  // 定数群
  // =================================================================
{PW_M10, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, -10);}, ""},
{PW_M3, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, -3);}, ""},
{PW_M2, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, -2);}, ""},
{PW_M1, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, -1);}, ""},
{PW_0, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 0);}, ""},
{PW_1, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 1);}, ""},
{PW_2, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 2);}, ""},
{PW_3, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 3);}, ""},
{PW_10, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 10);}, ""},
{PW_TRUE, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, true);}, ""},
{PW_FALSE, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, false);}, ""},
{PW_INPUT, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, INPUT);}, ""},
{PW_OUTPUT, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, OUTPUT);}, ""},
{PW_INPUT_PULLUP, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, INPUT_PULLUP);}, ""},
{PW_HIGH, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, HIGH);}, ""},
{PW_LOW, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, LOW);}, ""},
{PW_NULL, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, (int)NULL);}, ""},
{PW_A0, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A0);}, "(. -> A0) Analog port 0."},
{PW_A1, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A1);}, "(. -> A1) Analog port 1."},
{PW_A2, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A2);}, "(. -> A2) Analog port 2."},
{PW_A3, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A3);}, "(. -> A3) Analog port 3."},
{PW_A4, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A4);}, "(. -> A4) Analog port 4."},
{PW_A5, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A5);}, "(. -> A5) Analog port 5."},
{PW_A6, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A6);}, "(. -> A6) Analog port 6."},
{PW_A7, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A7);}, "(. -> A7) Analog port 7."},
{PW_A8, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A8);}, "(. -> A8) Analog port 8."},
{PW_A9, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A9);}, "(. -> A9) Analog port 9."},
{PW_A9, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, A9);}, "(. -> A9) Analog port 9."},
{PW_M7_RAM1, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 0x20000000);}, "(. -> addr) RAM1 area top address."},
{PW_M7_RAM2, false, [](FORTH_IP retIP){ return pwCONSTANT(retIP, 0x20200000);}, "(. -> addr) RAM2 area top address."},
  // =================================================================
  // Arduino関数群
  // =================================================================
{PW_DELAY, false, [](FORTH_IP retIP){
  STACK_ITEM  o = toIntNum(popStack(&dStack));
  delay(o.value.num);
  return  retIP;}, ""},
{PW_PINMODE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // mode
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // pin#
  pinMode(o2.value.num, o1.value.num);
  return  retIP;}, ""},
{PW_DIGITALWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // value
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // pin#
  digitalWrite(o2.value.num, o1.value.num);
  return  retIP;}, ""},
{PW_DIGITALREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // pin#
  int val = digitalRead(o1.value.num);
  STACK_ITEM  o2;
  o2.attr = ATTR_INTNUM;
  o2.value.num = val;
  pushStack(&dStack, o2);
  return  retIP;}, ""},
{PW_ANALOGWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // value
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // pin#
  analogWrite(o2.value.num, o1.value.num);
  return  retIP;}, ""},
{PW_ANALOGREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // pin#
  int val = analogRead(o1.value.num);
  STACK_ITEM  o2;
  o2.attr = ATTR_INTNUM;
  o2.value.num = val;
  pushStack(&dStack, o2);
  return  retIP;}, ""},
{PW_MILLIS, false, [](FORTH_IP retIP){
  unsigned  int tm = millis();
  pushDStackInt((int) tm);
  return  retIP;}, ""},
{PW_RANDOMSEED, false, [](FORTH_IP retIP){
  STACK_ITEM  o = toIntNum(popStack(&dStack));   // seed
  randomSeed(o.value.num);
  return  retIP;}, "(. -> .) Set new randomseed."},
{PW_RANDOM, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // max
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // min
  pushDStackInt(random(o2.value.num, o1.value.num));
  return  retIP;}, "(min, max -> random(min, max))"},
{PW_TONE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // freq
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // pin#
  tone(o2.value.num, o1.value.num);
  return  retIP;}, "(pin#, freq -> .) Tone without duration."},
{PW_TONED, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = toIntNum(popStack(&dStack));   // duration
  STACK_ITEM  o2 = toIntNum(popStack(&dStack));   // freq
  STACK_ITEM  o3 = toIntNum(popStack(&dStack));   // pin#
  tone(o3.value.num, o2.value.num, o1.value.num);
  return  retIP;}, "(pin#, freq, duration -> .) Tone with duration."},
{PW_NOTONE, false, [](FORTH_IP retIP){
  STACK_ITEM  o = toIntNum(popStack(&dStack));   // pin#
  noTone(o.value.num);
  return  retIP;}, "(pin# -> .) Stop tone."},
  // =================================================================
  // ワード定義の開始と終了。
  // =================================================================
{PWI_COLON, true, [](FORTH_IP retIP){
  if (isNewestHeaderIsNamedHeader()){
    HOST_PORT.println("Error: COLON in COLON.");
    prepareUnnamedCompile();  // ワード定義中にワード定義をしようとしたので、強制的に定義を終わらせる。
    return  (FORTH_IP)NULL;
  }
  setCompilerWaitName();
  return  retIP;}, ""},
{PWI_SEMICOLON, true, [](FORTH_IP retIP){
  compileStandardWord(searchDictionaryWithError(CW_POPIP));
  finishCompile();
  prepareUnnamedCompile();
  return  retIP;}, ""},
{PW_DUMPON, false, [](FORTH_IP retIP){
  approvalDump(true);
  return  retIP;}, "(. -> .) Object dump ON after compile."},
{PW_DUMPOFF, false, [](FORTH_IP retIP){
  approvalDump(false);
  return  retIP;}, "(. -> .) Object dump OFF after compile."},
  // =================================================================
  // 変数のためのワード群。
  // 変数もスタック同様にアトリビュートを保持できるように改良したため、数値だけではなく
  // 文字列も扱えるようにグレードアップした。
  // 文字列の保持と編集はmutableArrayRefで行い、その実体は*String[]であるため、
  // 柔軟な管理が得られた。旧形式のmalloc, realloc, freeを使う方式と違って、
  // メモリリークの不安もない。
  //
  // さらに配列も変数に保持するようにした。
  // =================================================================
{PW_SYSSTRCLR, false, [](FORTH_IP retIP){
  stringPool.clear();
  return  retIP;}, "(. -> .) Clear system string pool."},
{PW_SYSARYCLR, false, [](FORTH_IP retIP){
  stringPool.clear();
  return  retIP;}, "(. -> .) Clear system array pool."},
{PW_SYSSTKCLR, false, [](FORTH_IP retIP){
  initStack(&dStack);
  // 注: このレベルでリターンスタックを初期化してはいけない。
  initStack(&sStack);
  return  retIP;}, "(. -> .) Clear data and syntax stack."},
{PWI_VAR, true, [](FORTH_IP retIP){
  compileVar();
  return  retIP;}, ""},
{CW_VAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.attr = ATTR_INTNUM;
  o.value.ptr = retIP;
  pushStack(&dStack, o);
  STACK_ITEM  *ptr = (STACK_ITEM *)retIP;
  ptr++;
  ptr++;
  retIP = (FORTH_IP)ptr;
  return  retIP;}, ""},
{PW_VARREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (isAccessibleWithError(o.value.ptr, sizeof(STACK_ITEM)) == true){
    STACK_ITEM *varPtr = (STACK_ITEM *)o.value.ptr;  
    if (varPtr->attr == ATTR_STRINGREF){
      String *sptr = (String *)varPtr->value.ptr;
      pushDStackString(sptr->c_str());
    }else{
    // o = *ptr;     // これだと以降の無関係なブロックのコード生成までおかしくなる。
      o.attr = varPtr->attr;
      o.value.num = varPtr->value.num;
      pushStack(&dStack, o);
    }
  }
  return  retIP;}, "(VAR name -> data) 'data' is number or string."},
{PW_VARWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 変数に書き込むデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    STACK_ITEM  *varPtr = (STACK_ITEM *)o1.value.ptr;
    if (o2.attr == ATTR_STRING){                    // 文字列をstringPoolに追加して、Stringへの参照をVARに記録しようとしている(と解釈)。
      if (varPtr->attr != ATTR_STRINGREF){
        stringPool.append(String(""));
        varPtr->attr = ATTR_STRINGREF;
        varPtr->value.ptr = stringPool.peek(stringPool.count() - 1);
      }
      String *sptr = (String *)varPtr->value.ptr;
      *sptr = String((char *)o2.value.ptr);
    }else{
      // *ptr = o2;     // これだと以降の無関係なブロックのコード生成までおかしくなる。
      varPtr->attr = o2.attr;
      varPtr->value.num = o2.value.num;
    }
  }
  return  retIP;}, "(data, VAR name -> .) 'data' is number or string."},
{PW_VARADDWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 変数に加算するデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    STACK_ITEM  *varPtr = (STACK_ITEM *)o1.value.ptr;
    if (o2.attr == ATTR_STRING){                    // 文字列をstringPool[n]に追加しようとしている(と解釈)。
      if (varPtr->attr == ATTR_STRINGREF){
        String *sptr = (String *)varPtr->value.ptr;
        *sptr += String((char *)o2.value.ptr);
      }
    }else{
      // *ptr = o2;     // これだと以降の無関係なブロックのコード生成までおかしくなる。
      varPtr->attr = o2.attr;
      varPtr->value.num += o2.value.num;
    }
  }
  return  retIP;}, "(data, VAR name -> .) Add data to <VAR>, 'data' is number or string."},
  // =================================================================
  // 配列アクセスのためのワード群。
  // =================================================================
{PW_VARARRAYTOP, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    pushDStackPtr(iptr->peek(0));
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 配列のインデクス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    pushDStackInt(*(iptr->peek(o2.value.num)));
  }
  return  retIP;}, "(Index, VAR name -> data)"},
{PW_VARARRAYWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 配列のインデクス
  STACK_ITEM  o3 = popStack(&dStack);   // 配列に書き込むデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    iptr->replace(o2.value.num, o3.value.num);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYAPPEND, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o3 = popStack(&dStack);   // 配列に書き込むデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    iptr->append(o3.value.num);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYCLEAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    iptr->clear();
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYCOUNT, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<int> *iptr = setupIntArray(o1);
    pushDStackInt(iptr->count());
  }else{
    pushDStackInt(-1);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYBTOP, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    pushDStackPtr(iptr->peek(0));
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYBREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 配列のインデクス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    pushDStackInt(*(iptr->peek(o2.value.num)));
  }
  return  retIP;}, "(Index, VAR name -> data)"},
{PW_VARARRAYBWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o2 = popStack(&dStack);   // 配列のインデクス
  STACK_ITEM  o3 = popStack(&dStack);   // 配列に書き込むデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    iptr->replace(o2.value.num, o3.value.num);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYBAPPEND, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  STACK_ITEM  o3 = popStack(&dStack);   // 配列に書き込むデータ
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    iptr->append(o3.value.num);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYBCLEAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    iptr->clear();
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
{PW_VARARRAYBCOUNT, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);   // 変数のアドレス
  if (isAccessibleWithError(o1.value.ptr, sizeof(STACK_ITEM)) == true){
    mutableArray<byte> *iptr = setupByteArray(o1);
    pushDStackInt(iptr->count());
  }else{
    pushDStackInt(-1);
  }
  return  retIP;}, "(data, index, VAR name -> .)"},
  // =================================================================
  // メモリアクセスと文字列のためのワード群
  // =================================================================
{PW_MEMREAD, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (isAccessibleWithError(o.value.ptr, sizeof(int)) == true){
    o.value.num = *((int *)o.value.ptr);
    o.attr = ATTR_INTNUM;
  }  
  pushStack(&dStack, o);
  return  retIP;}, ""},
{PW_MEMWRITE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (isAccessibleWithError(o1.value.ptr, sizeof(int)) == true){
    *((int *)o1.value.ptr) = o2.value.num;
  }
  return  retIP;}, ""},
{PW_MEMREADW, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (isAccessibleWithError(o.value.ptr, sizeof(int16_t)) == true){
    o.value.num = *((int16_t *)o.value.ptr);
    o.attr = ATTR_INTNUM;
  }
  pushStack(&dStack, o);  
  return  retIP;}, ""},
{PW_MEMWRITEW, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (isAccessibleWithError(o1.value.ptr, sizeof(int16_t)) == true){
    *((int16_t *)o1.value.ptr) = o2.value.num;
  }
  return  retIP;}, ""},
{PW_MEMREADB, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);
  if (isAccessibleWithError(o.value.ptr, sizeof(char)) == true){
    o.value.num = *((char *)o.value.ptr);
    o.attr = ATTR_INTNUM;
  }
  pushStack(&dStack, o);  
  return  retIP;}, ""},
{PW_MEMWRITEB, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack);
  STACK_ITEM  o2 = popStack(&dStack);
  if (isAccessibleWithError(o1.value.ptr, sizeof(char)) == true){
    *((char *)o1.value.ptr) = o2.value.num;
  }
  return  retIP;}, ""},
  // =================================================================
  // 動的メモリ管理と文字列操作のためのワード群
  // =================================================================
/*
{PW_MEMALLOC, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);  // 割り当てサイズ
  o.value.ptr = malloc(o.value.num);
  pushStack(&dStack, o);
  return  retIP;}, ""},
{PW_MEMREALLOC, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack); // 現在の割り当てブロックへのポインタ
  STACK_ITEM  o2 = popStack(&dStack); // 割り当てサイズ
  void  *tmp = realloc(o1.value.ptr, o2.value.num);
  if (tmp == NULL){
    o1.value.ptr = NULL;
  }else{
    if (tmp != o1.value.ptr){
      o1.value.ptr = tmp;
    }
  }
  pushStack(&dStack, o1);
  return  retIP;}, ""},
{PW_MEMFREE, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);  // 現在の割り当てブロックへのポインタ
  free((void *)o.value.num);
  return  retIP;}, ""},
{PW_STRCOPY, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack); // コピー先へのポインタ
  STACK_ITEM  o2 = popStack(&dStack); // コピーする文字列へのポインタ
  strcpy((char *)o1.value.num, (char *)o2.value.num);
  return  retIP;}, "(String, String -> .)"},
{PW_STRCAT, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack); // 追加先へのポインタ(文字列先頭を指していること)
  STACK_ITEM  o2 = popStack(&dStack); // 追加する文字列へのポインタ
  strcat((char *)o1.value.num, (char *)o2.value.num);
  return  retIP;}, "(String, StringOrg -> .) Add a string to tail of stringOrg."},
{PW_STRCATC, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack); // 追加先へのポインタ(文字列先頭を指していること)
  STACK_ITEM  o2 = popStack(&dStack); // 追加する文字
  int len = strlen((char *)o1.value.num);
  char *addPtr = (char *)o1.value.num + len;
  *addPtr++ = (char)o2.value.num;
  *addPtr = '\0';
  return  retIP;}, "(Char, String -> .) Add a character to tail of string."},
{PW_STRLENGTH, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack);  // 文字列先頭へのポインタ
  pushDStackInt(strlen((char *)o.value.num));
  return  retIP;}, "(String -> Length)"},
*/
  // =================================================================
  // シリアルイベント関連のワード群
  // =================================================================
{PW_SERIALEVENT, false, [](FORTH_IP retIP){
  mySerialEvent();
  return  retIP;}, "(. -> .) Call me when need serial event."},
{PW_SERIALLENGTH, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.value.num = checkCharFromAux0Port();
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
  return  retIP;}, "(. -> Char count) Read a character count in AUX0 buffer."},
{PW_SERIALCHAR, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.value.num = readCharFromAux0Port();
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
  return  retIP;}, "(. -> Char) Read a character from AUX0."},
  // =================================================================
  // 辞書関連のワード群
  // =================================================================
{PW_FIND, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack); // 検索するワード名文字列
  D_WORD_HEADER *dWordHeader = searchDictionary((const char *)o.value.ptr);
  pushDStackPtr((void *)dWordHeader);
  return  retIP;}, "(Word name -> true / false) Find the word in dictionary."},
{PW_FORGET, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack); // 検索するワード名文字列
  D_WORD_HEADER *dWordHeader = searchDictionary((const char *)o.value.ptr);
  if (dWordHeader != NULL){
    forgetDictionary(dWordHeader);
  }
  pushDStackPtr((void *)dWordHeader);
  return  retIP;}, "(Word name -> dWordHeaderPtr / false) Forget the word."},
{PW_WORDS, false, [](FORTH_IP retIP){
  unsigned int wCount = dumpDictionary("");
  pushDStackInt(wCount);
  return  retIP;}, "(. -> Word count) Dump all words in dictionary."},
{PW_USAGE, false, [](FORTH_IP retIP){
  STACK_ITEM  o = popStack(&dStack); // 検索するワード名文字列
  dumpDictionary((const char *)o.value.ptr);
  return  retIP;}, "(. -> .) Print usage of word."},
{PW_UPDATEUSAGE, false, [](FORTH_IP retIP){
  STACK_ITEM  o1 = popStack(&dStack); // Usageをアップデートするワード名文字列
  STACK_ITEM  o2 = popStack(&dStack); // 新しいUsage文字列
  boolean   res = updateUsage((const char *)o1.value.ptr, (const char *)o2.value.ptr);
  pushDStackInt((int)res);
  return  retIP;}, "(New usage, Word name -> true/false) Update word usage."},
{PW_DICTFREE, false, [](FORTH_IP retIP){
  STACK_ITEM  o;
  o.value.num = dictionaryFree();
  o.attr = ATTR_INTNUM;
  pushStack(&dStack, o);
  return  retIP;}, "(. -> .) Print dictionary free by Decimal."},
{"", false, NULL, ""}
};

/*
 * プリミティブワードをすべて登録する。
 */
void    initPrimitive(){
  PRIMITIVE_WORD_INFO *wInfo = wordInfo;
  initAllStacks();
  while(wInfo->primitiveFunc != NULL){
    setupPrimitiveWord(wInfo++);
  }
  prepareUnnamedCompile();
}
/*
 * 起動時にStream.cpp内の初期化ルーチンでシリアル受信文字列としてセットされるコード。
 * 辞書を初期設定するのに使う定数テーブルにプリミティブワード自体を無名関数として登録したため、
 * プリミティブワードが別のプリミティブワードを呼び出すことができなくなった。このためこのような
 * ワードはここにFORTHレベルで記述することにした。
 */
const char *_initialWord[] = {
  ": SYSCLR SYS.STRING.CLEAR, SYS.ARRAY.CLEAR, SYS.STACK.CLEAR ;",
  ": CLEAR SYS.STACK.CLEAR ;",
  ": DDUP OVER OVER ;",
  ": 2DUP DUP DUP ;",
  ": ROT 2 ROLL ;",
  ": NULL? 0= ;",
  ": <= > 0= ;",
  ": >= < 0= ;",
  ": INRANGE ROT >R I >=, SWAP R> <=, & ;",
  ": 0! 0 SWAP ! ;",
  ": 0B! 0 SWAP B! ;",
  ": +V@",
  "  >R DUP V@ R> + DUP ROT V! ;",
  ": V@+",
  "  >R DUP V@ DUP R> + ROT V! ;",
  ": ++V@ 1 +V@ ;",
  ": V@++ 1 V@+ ;",
  ": --V@ -1 +V@ ;",
  ": V@-- -1 V@+ ;",
  ": H.8 H8 . ;",
  ": H.16 H16 . ;",
  ": H.32 H32 . ;",
  ": .CRLF . CRLF ;",
  ": .CRLF/H DUP INT? IF H.32 ELSE . THEN CRLF ;",
  ": S. DEPTH 0 DO .CRLF LOOP ;",
  ": S.. DEPTH 0 DO I PICK .CRLF LOOP ;",
  ": SH. DEPTH 0 DO .CRLF/H LOOP ;",
  ": SH.. DEPTH 0 DO I PICK .CRLF/H LOOP ;",
  ": FACT dup 1 > if dup 1 - fact * then ;",
  ": .SPACE 0x20 c. ;",
  ": DUMP.LINE  dup 0x7ffffff0 & dup h.32 \": \" . 16 0 do ddup > -if dup b@ h.8 else \"xx\" . then .space 1+ loop ;",
  ": DUMP.ASCII 16 - 0x7ffffff0 & 16 0 do ddup > -if dup b@ c.? else .space then  1+ loop ;",
  ": DUMP.GUIDE \"           +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F 0123456789ABCDEF\" . crlf ;",
  ": DUMP DUMP.GUIDE 4 0 do DUMP.LINE DUMP.ASCII crlf swap drop loop crlf drop ;",
  "\"(data1, data2 -> data1, data2, data1, data2)\" \"DDUP\" UPDATEUSAGE DROP",
  "\"(data1 -> data1, data1, data1)\" \"2DUP\" UPDATEUSAGE DROP",
  "\"(address (0xhhh.. or hhhh:hhhh -> .)) Dump memory 64 bytes from address.\" \"DUMP\" UPDATEUSAGE DROP",
  "#ECHO 1",
  NULL};

const char **  initialWord(){
  return  _initialWord;
}
