# DONGRI

Teensy 4.xで動作するFORTHコンパイラを書いてみました。

対話モードをコンパイル→実行→削除として実装することで、do〜loopなどの構文も実行することができるようにしました。初めてFORTHを知ったときから、これができるコンパイラを作りたいと思っていました。

多くのFORTHコンパイラはインタプリタ(対話)モードを持っており、定義済みのワードを呼び出して確認やテストをすることができます。ほとんどの実装はディクショナリからワードのエントリを引き出して呼び出すのですが、このためコンパイルしてオブジェクトを生成することが必須の制御文（IF - THENとかDO - LOOP）を呼び出すことができません。

ちょっと暇ができて、たまたま手元にTeensy 4.0があったので、やってみました。できるという確信はあったのですが、初めて動いたときは嬉しくて飛び上がりました。またTeensy 4という高性能なCPUを載せたArduino互換ボードがなかったら、Cで記述したディスパッチャで実用的な速度で動くものが作れるという自信は持てなかったかも知れません。


▇　お願い

早く結果を見たくて、力一杯書き倒したソースです。動作を知る参考にしてもプログラミングのお手本にはしないでください。しないか。しないよね。

おおよその構造についてはDOCフォルダの中にある図を見ていただくといいのですが、時間が経っているので書いた本人もわからなくなっているかも知れません。

end