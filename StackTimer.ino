#include <Adafruit_ST7735.h>

#include <vector>

#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "ssid";  //enter your SSID
const char* pass = "pass";  //enter your PassKey
const String url = "https://script.google.com/macros/s/AKfycbw6kTPpqkiyxYu9Vu6WqmADisxKyBfEYHivhF3TVhh3nZ7pDljCMXn-kJbZUhq420Df/exec";

/*RGB_LED*/
// PWM出力ピン
#define RED 13
#define GREEN 14
#define BLUE 12

// PWMチャンネル
#define PWM_CH_RED 0
#define PWM_CH_GREEN 1
#define PWM_CH_BLUE 2

/*Switch*/
#define SW_PIN_STACK 32   //INPUT_SWITCH stackスイッチ
#define SW_PIN_CONFIRM 25 //INPUT_SWITCH comfirmスイッチ（タクトスイッチ右）
#define SW_PIN_SELECT 26  //INPUT_SWITCH selectスイッチ（タクトスイッチ左）
#define BZ_PIN 27         //圧電ブザー

#define MODE_HOME 0
#define MODE_SINGLE 1
#define MODE_AO5 2
#define MODE_SEND 3

#define STATE_INSPECTION 0
#define STATE_SOLVE 1
#define STATE_RESULT 2

/*Display*/
#if defined(ESP32)
#define TFT_RST 4  // IO4
#define TFT_DC 2   // IO2
#define TFT_CS 5  // VSPICS0

#else
  #define TFT_CS        10
  #define TFT_RST        9
  #define TFT_DC         8
#endif

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

/*Timer*/
double W_displaytime = 0.0;     //表示用の時間
unsigned long W_time = 0;       //タイマーの値
unsigned long W_counttime = 0;  //経過時間
int W_msec = 0;                 //ミリ秒の保存
int W_sec = 0;                  //秒の保存
int W_min = 0;                  //分の保存

int State_Num = 0;                //何回押したか。"0"ならスタート、"1"ならストップ＆表示
int Mode_Num = MODE_HOME;         //モード選択。１回目ならSINGLE、２回目ならAO5、３回目ならAO12
bool Push_Flg_State = false;      //現在押しているかのフラグ（タイマーボタン）
bool Push_Flg_Mode = false;       //現在押しているかのフラグ（モードボタン）
bool Push_Flg_Confirm = false;    //現在押しているかのフラグ（決定ボタン）
bool Time_Measureing_Flg = false; //タイム計測画面中か

int AO5_Count = 1;
unsigned long Current_Ave = 0;
unsigned long AO5_Record[5] = {0, 0, 0, 0, 0};
unsigned long Record_Min = 0;
unsigned long Record_Max = 0;

int Sensor_Judge = 0;       //センサーの判断用
bool Switch_Judge = false;  //スイッチの判断用

/*record*/
String record_Time;
std::vector<String> record_Times;
std::vector<String> record_Mode;


void setup() {
  Serial.begin(115200);

  //LEDの設定
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  ledcSetup(PWM_CH_RED, 1000, 8);
  ledcAttachPin(RED, PWM_CH_RED);
  ledcSetup(PWM_CH_GREEN, 1000, 8);
  ledcAttachPin(GREEN, PWM_CH_GREEN);
  ledcSetup(PWM_CH_BLUE, 1000, 8);
  ledcAttachPin(BLUE, PWM_CH_BLUE);

  ledcWrite_BLUE();

  //スイッチの設定
  pinMode(SW_PIN_SELECT, INPUT_PULLUP);
  pinMode(SW_PIN_CONFIRM, INPUT_PULLUP);

  //圧電ブザー設定
  pinMode(BZ_PIN, OUTPUT);

  //液晶の設定
  tft.initR(INITR_GREENTAB);
  tft.setRotation(1);
  tft.setTextWrap(false);
  tft.setTextColor(ST7735_GREEN);
  tft.setTextSize(2);

  //初期状態
  Mode_Num = MODE_HOME;
  Push_Flg_Mode = true;
}

void loop() {
  /********************/
  /*MODE SELECT Screen*/
  /********************/
  while(Push_Flg_Confirm != true){
    if(Push_Flg_Mode == true){
      switch(Mode_Num){
      case MODE_HOME:
        ledcWrite_BLUE();
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 20); tft.print("SINGLE");
        tft.setCursor(10, 50); tft.print("AO5");
        tft.setCursor(10, 80); tft.print("SEND");

        break;
      case MODE_SINGLE:
        tft.drawRect(6, 76, 78, 22, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(5, 75, 80, 24, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(6, 16, 78, 22, ST7735_BLUE); //選択枠の描画
        tft.drawRect(5, 15, 80, 24, ST7735_BLUE); //選択枠の描画
        
        break;
      case MODE_AO5:
        tft.drawRect(6, 16, 78, 22, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(5, 15, 80, 24, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(6, 46, 78, 22, ST7735_BLUE); //選択枠の描画
        tft.drawRect(5, 45, 80, 24, ST7735_BLUE); //選択枠の描画
        
        break;
      case MODE_SEND:
        tft.drawRect(6, 46, 78, 22, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(5, 45, 80, 24, ST7735_BLACK); //一つ前の枠を消す
        tft.drawRect(6, 76, 78, 22, ST7735_BLUE); //選択枠の描画
        tft.drawRect(5, 75, 80, 24, ST7735_BLUE); //選択枠の描画
        
        break;
      }
      delay(200);
    }
    Read_SW_SELECT();

    Read_SW_CONFIRM();
    if(Push_Flg_Confirm == 1){
      if(Mode_Num == MODE_HOME){
        while(digitalRead(SW_PIN_CONFIRM) == LOW){}
        Push_Flg_Confirm = false;
      }else if(Mode_Num == MODE_SINGLE){
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("PUSH TO");
        tft.setCursor(10, 70); tft.print("START");
        while(digitalRead(SW_PIN_CONFIRM) == LOW){}
        Push_Flg_Confirm = false;
      }else if(Mode_Num == MODE_AO5){
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("PUSH TO");
        tft.setCursor(10, 70); tft.print("START");
        while(digitalRead(SW_PIN_CONFIRM) == LOW){}
        Push_Flg_Confirm = false;
      }else if(Mode_Num == MODE_SEND){
        while(digitalRead(SW_PIN_CONFIRM) == LOW){}
        Push_Flg_Confirm = false;
      }

      break;
    }
  }

  /***************/
  /*SINGLE Screen*/
  /***************/
  while(Mode_Num == MODE_SINGLE && Push_Flg_Confirm != true){
    //タイマーステータス
    if(Push_Flg_State == true){
      Push_Flg_State = !Push_Flg_State;

      switch(State_Num){
      //インスペクションタイム待機状態
      case STATE_INSPECTION:
        ledcWrite_GREEN();
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("INSPECTION");
        tft.setCursor(10, 70); tft.print("READY...");

        inspectionTimer();

        break;

      //測定待機状態
      case STATE_SOLVE:
        ledcWrite_BLUE();
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("SOLVE");
        tft.setCursor(10, 70); tft.print("READY...");

        solveTimer();

        break;

      //測定停止状態
      case STATE_RESULT:
        ledcWrite_RED();

        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 20); tft.print("STOP");

        //結果の表示
        tft.setCursor(10, 70);
        tft.setTextSize(3);
        record_Format();
        tft.print(record_Time);
        tft.setTextSize(2);

        //結果の保存
        record_Times.push_back(record_Time);
        record_Mode.push_back("SINGLE");

        delay(500);  //結果表示は1秒維持
        while(analogRead(SW_PIN_STACK) >= 1000){} //押しっぱなしの間は進まない

        break;
      }
      State_Num++;
      if(State_Num > STATE_RESULT){
        State_Num = STATE_INSPECTION;
      }
    }

    //ReadSwitch
    Read_SW_STACK();

    Read_SW_CONFIRM();
    if(Push_Flg_Confirm == true){
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 50); tft.print("BACK");
      delay(500);

      Mode_Num = MODE_HOME;
      Push_Flg_Mode = true;
      Push_Flg_Confirm = false;

      break;
    }
  }

  /************/
  /*AO5 Screen*/
  /************/
  while(Mode_Num == MODE_AO5 && Push_Flg_Confirm != true){
    //タイマーステータス
    if(Push_Flg_State == true){
      Push_Flg_State = !Push_Flg_State;

      switch(State_Num){
      //インスペクションタイム待機状態
      case STATE_INSPECTION:
        ledcWrite_GREEN();
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 30); tft.print(AO5_Count); tft.print("/5");
        tft.setCursor(10, 50); tft.print("INSPECTION");
        tft.setCursor(10, 70); tft.print("READY...");

        inspectionTimer();

        break;

      //測定待機状態
      case STATE_SOLVE:
        ledcWrite_BLUE();
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 30); tft.print(AO5_Count); tft.print("/5");
        tft.setCursor(10, 50); tft.print("SOLVE");
        tft.setCursor(10, 70); tft.print("READY...");

        solveTimer();

        break;

      //測定停止状態
      case STATE_RESULT:
        ledcWrite_RED();
        
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 20); tft.print("STOP");
        tft.setCursor(10, 50); tft.print(AO5_Count); tft.print("/5");

        /*平均値の算出*/
        Current_Ave = 0;
        AO5_Record[AO5_Count - 1] = W_counttime;
        for(int i = 0; i < AO5_Count; i++){
          Current_Ave += AO5_Record[i];
        }

        //Min・Maxの判定
        if(AO5_Count == 1){
          Record_Min = AO5_Record[0];
          Record_Max = AO5_Record[0];
        }else{
          Record_Min = min(Record_Min, AO5_Record[AO5_Count - 1]);
          Record_Max = max(Record_Max, AO5_Record[AO5_Count - 1]);
        }

        /*結果の表示*/
        tft.setCursor(10, 70);
        tft.setTextSize(3);
        record_Format();
        tft.print(record_Time);
        tft.setTextSize(2);

        /*平均値の表示*/
        if(AO5_Count < 5){
          W_displaytime = Current_Ave / AO5_Count;
          tft.setTextSize(1);
          tft.setCursor(10, 100); tft.print("Current");
          tft.setCursor(10, 110); tft.print("Average");
          tft.setTextSize(2);
          tft.setCursor(55, 100); tft.print(":"); tft.print(W_displaytime / 1000);
        }else{
          W_counttime = (Current_Ave - Record_Min - Record_Max) / 3;
          record_Format();

          tft.setTextSize(1);
          tft.setCursor(10, 105); tft.print("Result");
          tft.setTextSize(2);
          tft.setCursor(50, 100); tft.print(":"); tft.print(record_Time);

          //結果の保存
          record_Times.push_back(record_Time);
          record_Mode.push_back("AO5");
        }

        delay(500);  //結果表示は1秒維持
        while(analogRead(SW_PIN_STACK) >= 1000){} //押しっぱなしの間は進まない
        
        AO5_Count++;
        if(AO5_Count > 5){
          AO5_Count = 1;
        }

        break;
      }
      State_Num++;
      if(State_Num > STATE_RESULT){
        State_Num = STATE_INSPECTION;
      }
    }

    //ReadSwitch
    Read_SW_STACK();

    Read_SW_CONFIRM();
    if(Push_Flg_Confirm == true){
      AO5_Count = 1;

      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 50); tft.print("BACK");
      delay(500);

      Mode_Num = MODE_HOME;
      Push_Flg_Mode = true;
      Push_Flg_Confirm = false;
      
      break;
    }
  }
  

  /*************/
  /*SEND Screen*/
  /*************/
  while(Mode_Num == MODE_SEND && Push_Flg_Confirm != true){
    if(!record_Times.empty()){
      WiFi.disconnect();
      if(WiFi.begin(ssid, pass) != WL_DISCONNECTED){
        ESP.restart();
      }

      while(WiFi.status() != WL_CONNECTED){
        Serial.println("Trying to connect");

        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(10, 20); tft.print("Trying");
        tft.setCursor(10, 40); tft.print("to connect");

        //接続できない時の脱出用
        Read_SW_CONFIRM();
        if(Push_Flg_Confirm == 1){
          tft.fillScreen(ST7735_BLACK);
          tft.setCursor(10, 50); tft.print("BACK");
          delay(1000);

          Mode_Num = MODE_HOME;
          Push_Flg_Mode = true;

          Push_Flg_Confirm = false;
          break;
        }
        delay(1000);
      }
      //接続成功時
      Serial.println("WiFi Connected");
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 20); tft.print("Connected");

      /*record送信*/
      int TimesSize = record_Times.size();
      int recordCnt = 1;
      int httpCode;   //送信成功・失敗判定用
      String payload; //送信成功・失敗判定用

      tft.setCursor(10, 50); tft.print("sending...");

      while(!record_Times.empty()){
        String urlFinal = url + "?record=" + String(record_Times.front()) + "&mode=" + String(record_Mode.front());
        Serial.println(urlFinal);

        tft.fillRect(0, 70, 160, 30, ST7735_BLACK);
        tft.setTextSize(3);
        tft.setCursor(10, 70); tft.print(recordCnt); tft.print("/"); tft.print(TimesSize);
        tft.setTextSize(2);

        HTTPClient http;
        http.begin(urlFinal.c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        httpCode = http.GET(); 
        Serial.print("HTTP Status Code : ");
        Serial.println(httpCode);
        //---------------------------------------------------------------------
        //getting response from google sheet
        if (httpCode > 0) {
            payload = http.getString();
            Serial.println("Payload: "+payload);
        }
        //---------------------------------------------------------------------
        if(httpCode < 0 || !payload.equals("success")){   //送信がうまくいかなかったら中断
          break;
        }

        record_Times.erase(record_Times.begin());
        record_Mode.erase(record_Mode.begin());
        recordCnt++;

        http.end();
      }

      if(httpCode < 0){
        tft.fillRect(0, 50, 160, 50, ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("sending");
        tft.setCursor(10, 70); tft.print("failed !");

        delay(1000);
      }else{
        tft.fillRect(0, 50, 160, 50, ST7735_BLACK);
        tft.setCursor(10, 50); tft.print("record was");
        tft.setCursor(10, 70); tft.print("successfully");
        tft.setCursor(10, 90); tft.print("sent.");
        delay(1500);

        tft.fillRect(0, 50, 160, 60, ST7735_BLACK);
      }
    
    }else{
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 50); tft.print("There is");
      tft.setCursor(10, 70); tft.print("no record");
      delay(1000);
      
      Mode_Num = MODE_HOME;
      Push_Flg_Mode = true;

      Push_Flg_Confirm = false;
      break;
    }
    
    Read_SW_CONFIRM();
    if(Push_Flg_Confirm == true){
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(10, 50); tft.print("BACK");
      delay(1000);

      Mode_Num = MODE_HOME;
      Push_Flg_Mode = true;

      Push_Flg_Confirm = false;
      break;
    }
  }
}


/********************/
/*Read Switch Method*/
/********************/
/*STACK*/
void Read_SW_STACK(){
  Sensor_Judge = analogRead(SW_PIN_STACK);    //圧力センサーが押されたかを確認する
  if(Sensor_Judge >= 1000){                   //押下圧が1000以上か
    delay(10);                                //チャタリングじゃないかを判断するために10ms後も押し続けたかで判断
    Sensor_Judge = analogRead(SW_PIN_STACK);  //センサーが押されたかを確認する
    if(Sensor_Judge >= 1000){                 //押下圧が1000以上か
      Push_Flg_State = true;
    }else{
      Push_Flg_State = false;
    }
  }else{
    Push_Flg_State = false;
  }
}

/*SELECT*/
void Read_SW_SELECT(){
  Switch_Judge = digitalRead(SW_PIN_SELECT);    //ボタンが押されたかを確認する
  if(Switch_Judge == LOW){
    delay(40);                                  //チャタリングじゃないかを判断するために10ms後も押し続けたかで判断
    Switch_Judge = digitalRead(SW_PIN_SELECT);  //ボタンが押されたかを確認する
    if(Switch_Judge == LOW){
      Push_Flg_Mode = true;
      Mode_Num++;
      if(Mode_Num > MODE_SEND){
        Mode_Num = MODE_SINGLE;
      }
    }else{
      Push_Flg_Mode = false;
    }
  }else{
    Push_Flg_Mode = false;
  }
}

/*CONFIRM*/
void Read_SW_CONFIRM(){
  Switch_Judge = digitalRead(SW_PIN_CONFIRM);    //ボタンが押されたかを確認する
  if(Switch_Judge == LOW){
    delay(40);                                   //チャタリングじゃないかを判断するために10ms後も押し続けたかで判断
    Switch_Judge = digitalRead(SW_PIN_CONFIRM);  //ボタンが押されたかを確認する
    if(Switch_Judge == LOW){
      Push_Flg_Confirm = true;
    }else{
      Push_Flg_Confirm = false;
    }
  }else{
    Push_Flg_Confirm = false;
  }
}


/*************************/
/*RGB_LED Lighting Method*/
/*************************/
void ledcWrite_RED(){
  ledcWrite(PWM_CH_RED, 31);
  ledcWrite(PWM_CH_GREEN, 0);
  ledcWrite(PWM_CH_BLUE, 0);
}

void ledcWrite_GREEN(){
  ledcWrite(PWM_CH_RED, 0);
  ledcWrite(PWM_CH_GREEN, 15);
  ledcWrite(PWM_CH_BLUE, 0);
}

void ledcWrite_BLUE(){
  ledcWrite(PWM_CH_RED, 0);
  ledcWrite(PWM_CH_GREEN, 0);
  ledcWrite(PWM_CH_BLUE, 31);
}

/**********************/
/*Record Format Method*/
/**********************/
void record_Format(){
  W_msec = W_counttime % 1000;      //1000で割った余りをmsにする
  W_counttime = W_counttime /1000;  //経過時間を1000で割って最小単位を秒にする。
  W_min = W_counttime / 60;         //経過時間を60で割って分を導く。
  W_sec = W_counttime % 60;         //経過時間を60で割った余りを秒にする。

  record_Time = "";
  if(W_min > 0){
    record_Time = (String)W_min + ":";      //1分未満の時は表示しない
    if(W_sec < 10){
      (String)record_Time += "0";           //10秒未満の時は先頭に0を表示
    }
  }
  record_Time += (String)W_sec + ".";
  if(W_msec < 10){
    record_Time += "00";                    //10ミリ秒未満の時は先頭に00を表示
  }else if(W_msec < 100){
    record_Time += "0";                     //100ミリ秒未満の時は先頭に0を表示
  }
  record_Time += (String)W_msec;
}


/**************/
/*Timer Method*/
/**************/
/*INSPECTION TIME*/
void inspectionTimer(){
  while(analogRead(SW_PIN_STACK) >= 1000){} //押しっぱなしの間は進まない
  W_time = millis();                        //開始時間を記録

  while(Push_Flg_State != 1){
    Read_SW_STACK();
    W_displaytime = millis() - W_time;

    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(10, 20); tft.print("INSPECTION");
    tft.setCursor(10, 70);
    tft.setTextSize(3);
    tft.print(W_displaytime / 1000);
    tft.setTextSize(2);

    //ブザー鳴動
    if((W_displaytime / 1000) > 9.0 && (W_displaytime / 1000) <= 10.0){         //10秒になることを知らせるブザー
      digitalWrite(BZ_PIN, HIGH);
    }else if((W_displaytime / 1000) > 14.0 && (W_displaytime / 1000) <= 15.0){  //15秒になることを知らせるブザー
      digitalWrite(BZ_PIN, HIGH);
    }else{
      digitalWrite(BZ_PIN, LOW);
    }

    //15秒を超えたら文字色を赤に変更
    if((W_displaytime / 1000) > 15.0){
      tft.setTextColor(ST7735_BLUE);  //「BLUE」だが実際は赤色
    }
  }
  digitalWrite(BZ_PIN, LOW);          //ブザー停止
  tft.setTextColor(ST7735_GREEN);     //文字色を緑に戻す
}

/*SOLVE TIME*/
void solveTimer(){
  while(analogRead(SW_PIN_STACK) >= 1000){} //押しっぱなしの間は進まない
  W_time = millis();                        //開始時間を記録

  while(Push_Flg_State != 1){
    Read_SW_STACK();
    W_displaytime = millis() - W_time;

    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(10, 20); tft.print("START");

    tft.setCursor(10, 70);
    tft.setTextSize(3);
    tft.print(W_displaytime / 1000);
    tft.setTextSize(2);

    //RGB_LED Lightning
    if((int)W_displaytime % 200 < 100){
      ledcWrite_RED();
    }else{
      ledcWrite_GREEN();
    }

    W_counttime = millis() - W_time; //経過時間を導く
  }
}