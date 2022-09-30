#include  <arduino.h>
#include  "StreamIO.h"
#include  "Utility.h"
#include  "System.h"
#include  "Primitive.h"


String    streamBuf_HOST = "";
String    streamBuf_AUX0 = "";

void    prepareInitialWord(){
  const char **_initialWord = initialWord();
  while (*_initialWord != NULL){
    streamBuf_HOST += String(*_initialWord++) + String("\n");
  }
}
void    initHostPort(){
  HOST_PORT.begin(HOST_PORT_BAUD);
  if (HOST_PORT != AUX0_PORT)
    AUX0_PORT.begin(AUX0_PORT_BAUD);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  while(!Serial.dtr()); //  Wait for Serial-monitor ready.
  digitalWrite(13, LOW);
  prepareInitialWord();
}
/*
 * ホストに起動メッセージを送る。バージョン管理の代わりにビルドした日付と時刻を生成している。
 */
void    openingMessageToHostPort(){
  HOST_PORT.print("DONGRI a FORTH Compiler for Teensy®4.0 ver 0.9.1 (Build Date = ");
  HOST_PORT.print(getFormatedDate(__DATE__));
//  HOST_PORT.print(" , Time = ");
//  HOST_PORT.print(__TIME__);
  HOST_PORT.println(")");
}
/*
 * シリアルポートにイベントが発生した場合にコールバックされる関数のようだ。
 * こんなのあること知らなかったので、正しい使い方かどうか自信がない。
 * その後の調査で、使い方自体は間違っていないことはわかった。ただしこれは
 * 割り込み処理のコールバック関数のようなものではなく、loop()を抜けたタイミングで
 * 呼び出されるポーリング処理のようだ。
 */
void    serialEvent() {
  while (HOST_PORT.available()) {
    char inChar = (char)HOST_PORT.read();
    streamBuf_HOST += inChar;
  }
}
/*
 * こちらはFORTHプログラムの中でシリアルデータ（キー入力を含む）を受け付けるため。
 * イベント待ちのループの中ではこれを定期的に呼び出してシリアルイベントに対応する。
 */
void    mySerialEvent(){
  while (AUX0_PORT.available()) {
    char inChar = (char)AUX0_PORT.read();
    streamBuf_AUX0 += inChar;
  }
}
/*
 * streamBufの先頭から終端子の直前までの1行を返す。
 * 終端子を発見した場合に呼び出されるので、戻り値は必ず正しいStringである。
 * streamBufは切り出した1行の分だけ先頭を削除される。
 */
String  readLineFromStreamBuf(String *streamBuf, int idx){
  String  res = streamBuf->substring(0, idx);
  streamBuf->remove(0, idx); 
  return  res; 
}
/*
 * ホストから受信したデータの先頭の1行を切り出して返す。
 * 行は"\r\n"または"\r"あるいは"\n"で終端されていると想定。
 * これらの終端子が見つからない場合はstreamBufに文字列があっても行とは見なさない。
 * 行があった場合、受信データの先頭から終端子を含んだ文字列を返す。したがって、
 * 行の文字数は必ず1以上になる。もし文字数が0ならば受信データがないか、あるいは
 * 受信バッファに終端子がまだ届いていないことを表す。
 */
String  readLineFromPort(String *streamBuf){
  int   idx = 0;
  idx = streamBuf->indexOf("\r\n");
  if (idx >= 0)
    return  readLineFromStreamBuf(streamBuf, idx + 2);
  idx = streamBuf->indexOf("\r");
  if (idx >= 0)
    return  readLineFromStreamBuf(streamBuf, idx + 1);
  idx = streamBuf->indexOf("\n");
  if (idx >= 0)
    return  readLineFromStreamBuf(streamBuf, idx + 1);
  return   String("");
}
String  readLineFromHostPort(){
  return  readLineFromPort(&streamBuf_HOST);
}
String  readLineFromAux0Port(){
  return  readLineFromPort(&streamBuf_AUX0);
}
char    readCharFromAux0Port(){
  if (streamBuf_AUX0.length() == 0){
    return  '\0';
  }
  char  c = streamBuf_AUX0.charAt(0);
  streamBuf_AUX0.remove(0, 1);
  return  c;
}
int     checkCharFromAux0Port(){
  return  streamBuf_AUX0.length();
}
