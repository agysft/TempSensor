# Temperature and humidity logger
![image](image/00.JPG)
This is a prototype of a device that records temperature and humidity to an SD card every hour.  
これは１時間毎に温度と湿度をSDカードに記録する装置の試作です。  
It is actually a thermo-hygrometer to control the temperature of leopard geckos. It is intended to be hooked up to a breeding case.  
![image](image/overview.JPG)
実はヒョウモントカゲモドキの温度管理をするための温湿度計です。
飼育ケースに引っ掛けて使うことを想定しています。
The system is powered on and off at regular intervals by RTC and automatically turns off when the measurement is finished.  
RTCで一定時間毎に電源を入れて計測し、計測終了したら自動で電源を切る仕組みです。  
OLEDs can also be mounted. I chose Arduino base because it has a library of fonts.  
OLEDも載せられる様にしてます。その時フォントとか選びたいのでライブラリがあるArduinoベースにしました。  
![image](image/02.JPG)
The program was written in Arduino IDE. The schematic is written in KiCAD and the printed circuit board is made. The structure was made with a 3D printer.  
プログラムはArduino IDEで書きました。回路図はKiCADで書き、プリント基板を作っています。構造物は3Dプリンターで作りました。  
![image](image/01.JPG)
![image](image/03.JPG)

---

* [schematics](schematics/TempHumiSensor.pdf)

1時間毎に自動測定する仕組み.  How to measure automatically every hour
1. RTCで1時間毎に/INT信号を出す。RTC sends /INT signal every hour.
2. /INT信号がLowになると、Q1がOn=電源On。When the /INT signal goes Low, Q1 turns On = power supply On.
3. 電源がOnになると温湿度を測定する。  When the power supply is turned On, the temperature and humidity are measured.  
4. 測定終了したら/INTをクリア、Q1がOff=電源Off。  When measurement is completed, /INT is cleared and Q1 turns Off = power is Off.  
