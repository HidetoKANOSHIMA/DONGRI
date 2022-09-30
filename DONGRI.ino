#include    "StreamIO.h"
#include    "System.h"
#include    "Parser.h"
#include    "Primitive.h"
#include    "Dictionary.h"
#include    "Utility.h"

Parser      DGParser;   // パーサは付加機能なのでclassとして独立させた。

void setup() {
  // put your setup code here, to run once:
  approvalDump(false);  // コンパイル後のオブジェクトダンプを制御。trueもしくはワード「DUMPON」でダンプ。
  approvalRUN(true);    // コンパイル即実行の機能を制御。trueで即実行。
  initHostPort();       // シリアルモニタがアクティブになるまで帰ってこない。
  openingMessageToHostPort();
  initSystem();         // FORTHシステムを初期化する。
  getFreeITCM();
}
/*
 * ターミナルのテキストファイル送信機能でソースを流し込んでいる際にエラーが発生したら
 * それ以降のソース行を無視する必要がある。手入力でもシリアルから入って来るので区別が
 * 付けられないから、少し工夫をしている。
 */
void  waitForStabilize(){
  delay(100);
  serialEvent();
  String  lineRes = readLineFromHostPort();
  if (lineRes.length() == 0){
    return;                     // 手入力でエラーが発生した場合はここで帰る。
  }
  HOST_PORT.print("waitForStabilize..");
  delay(500);                   // ターミナルからの後続テキストがたまるのを少し待つ。
  serialEvent();
  lineRes = readLineFromHostPort();
  while(lineRes.length() > 0){
    HOST_PORT.print(".");
    delay(100);
    serialEvent();
    lineRes = readLineFromHostPort();
  }
  HOST_PORT.println("done.");
}

/*
 * ホストからの受信データがない場合はこの中で待つことはしない。
 * varboseは、入力ラインのエコーバックとプロンプトを制御する。
 * 起動時にPremitive.cpp内の組み込みFORTHワードをコンパイルする間、静かにしていてもらうため。
 */
void loop() {
  // put your main code here, to run repeatedly:
  String  lineRes = readLineFromHostPort();
  if (lineRes.length() > 0){
    boolean normalEnd = true;
    int cnt = DGParser.parser(String(lineRes));
    if (cnt >= 0){
      for (int i = 0; i < cnt; i++){
        String  str = DGParser.getWordPool(i);
        const char *wordName = str.c_str();
        normalEnd = forthEntry(wordName);
        if (normalEnd == false)
          break;
      }
      if (normalEnd == true){
        forthEntry(NULL);
      }else{
        waitForStabilize();
      }
      if (DGParser.isInputEcho() || DGParser.isMacroEcho())
        putPrompt();
    }
  }
}
