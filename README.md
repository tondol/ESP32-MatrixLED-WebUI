ESP32-MatrixLED-WebUI
====

これはなに
----

ESP-WROOM32-EとHUB75対応のマトリックスLEDパネル (64x64) を組み合わせてWeb UIでアニメGIFの再生をできるようにするプロジェクトです。

参考元

- https://github.com/mzashh/HUB75-Pixel-Art-Display/
- https://ogimotokin.hatenablog.com/entry/2020/12/19/100000

主な変更点
----

- 128x64対応
- 自前LEDパネル用の設定適用
    - RGBの信号線を再アサイン
    - clkphase = false
- パーティションを16MB向けに変更
- ESP32コアとファイルシステム書き込みの周波数をUP
- ライフサイクルと状態遷移の見直し
    - GIFアニメの切り替えを高速化 & 安定化
    - HTMLのGIFアニメプレビューを高速化 & 安定化
- SPIFFS -> LittleFS
- 文字再生機能を追加
