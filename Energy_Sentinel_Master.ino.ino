// ==========================================
//    THE ENERGY SENTINEL (FINAL MASTER)
//    Team: Error Terror | Amit Vishwakarma
// ==========================================

#define BLYNK_TEMPLATE_ID "TMPL3eqQLQWU8"
#define BLYNK_TEMPLATE_NAME "Smart Energy Meter"
#define BLYNK_AUTH_TOKEN "2QGnwkprn7-cxIZ1l-f9s3xNe1wfTzl0"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// 👇 LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// 👇 WIFI DETAILS 👇
char ssid[] = "Error";   
char pass[] = "123456789";  

#define DHTPIN 32     
#define DHTTYPE DHT22 
DHT dht(DHTPIN, DHTTYPE);

// --- PINS SETUP ---
const int pirPin = 13;       
const int lightPin = 5;      
const int fanPin = 18;       
const int ldrPin = 33;       
const int voltSensor = 34;   
const int currSensor = 35;   
const int physicalLightBtn = 4;  
const int physicalFanBtn = 26;   // Updated to Pin 26 

// --- VARIABLES ---
unsigned long lastMotionTime = 0;       
const long stayOnTime = 60000;  // 1 Min Auto-off timer
bool isSystemActive = false;    
bool isDark = false;            
bool emergencyMode = false;     
const float TEMP_THRESHOLD = 30.0; 
bool notificationSent = false;  

// Button States
int manualFanState = HIGH;      
bool physicalLightOn = false;   
bool physicalFanOn = false;     
int lastLightBtnState = HIGH;
int lastFanBtnState = HIGH;

BlynkTimer timer;

// V5: App Fan Switch
BLYNK_WRITE(V5) { 
  int appValue = param.asInt(); 
  manualFanState = (appValue == 1) ? LOW : HIGH; 
}

// V10: MASTER KILL SWITCH
BLYNK_WRITE(V10) {
  int value = param.asInt();
  if (value == 1) {
    emergencyMode = true; 
    isSystemActive = false;
    physicalLightOn = false;
    physicalFanOn = false;
    manualFanState = HIGH;
    
    digitalWrite(lightPin, HIGH); 
    digitalWrite(fanPin, HIGH); 
    
    Blynk.virtualWrite(V0, 0); Blynk.virtualWrite(V5, 0); 
    Blynk.virtualWrite(V8, 0); Blynk.virtualWrite(V7, 0);
    Blynk.virtualWrite(V9, "⛔ EMERGENCY STOP");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("! EMERGENCY !");
    lcd.setCursor(0, 1);
    lcd.print("SYSTEM HALTED");
  } else {
    emergencyMode = false; 
    Blynk.virtualWrite(V9, "✅ System Ready");
    lcd.clear();
  }
}

// Function: Taar wale Switches
void checkPhysicalButtons() {
  if (emergencyMode) return;

  int currentLightBtn = digitalRead(physicalLightBtn);
  int currentFanBtn = digitalRead(physicalFanBtn);

  if (currentLightBtn == LOW && lastLightBtnState == HIGH) {
    physicalLightOn = !physicalLightOn; 
    Blynk.virtualWrite(V0, physicalLightOn ? 1 : 0); 
    Blynk.virtualWrite(V8, physicalLightOn ? 255 : 0);
    delay(400); 
  }
  lastLightBtnState = currentLightBtn; 

  if (currentFanBtn == LOW && lastFanBtnState == HIGH) {
    physicalFanOn = !physicalFanOn; 
    Blynk.virtualWrite(V5, physicalFanOn ? 1 : 0); 
    delay(400); 
  }
  lastFanBtnState = currentFanBtn;
}

// Function: Sensor Data Bhejna
void sendSensorData() {
  if (emergencyMode) return;

  bool motion = digitalRead(pirPin);
  Blynk.virtualWrite(V4, motion); 
  
  float temp = dht.readTemperature(); 
  if (!isnan(temp)) Blynk.virtualWrite(V6, temp);

  int vRaw = analogRead(voltSensor);
  int cRaw = analogRead(currSensor);
  float voltage = map(vRaw, 0, 4095, 200, 240); 
  float current = map(cRaw, 0, 4095, 0, 1000) / 100.0; 
  float power = voltage * current; 
  
  Blynk.virtualWrite(V1, voltage);
  Blynk.virtualWrite(V2, current);
  Blynk.virtualWrite(V3, power);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(pirPin, INPUT);
  pinMode(ldrPin, INPUT); 
  pinMode(lightPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(physicalLightBtn, INPUT_PULLUP);
  pinMode(physicalFanBtn, INPUT_PULLUP);
  
  digitalWrite(lightPin, HIGH); 
  digitalWrite(fanPin, HIGH); 

  // LCD STARTUP MAGIC
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ENERGY SENTINEL");
  lcd.setCursor(0, 1);
  lcd.print("SYSTEM BOOTING..");
  
  dht.begin(); 
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  timer.setInterval(1000L, sendSensorData); 
  Blynk.virtualWrite(V9, "System Ready"); 
  lcd.clear();
}

void loop() {
  Blynk.run(); 
  timer.run();
  checkPhysicalButtons(); 

  if (emergencyMode) return; 

  bool motion = digitalRead(pirPin);
  int ldrState = digitalRead(ldrPin); 
  float temp = dht.readTemperature(); 
  isDark = (ldrState == HIGH) ? true : false;

  // 1. PIR TIMER LOGIC
  if (motion == HIGH) {
    lastMotionTime = millis();
    isSystemActive = true;
    if (!notificationSent) {
      Blynk.logEvent("motion_alert", "⚠️ Motion Detected!"); 
      notificationSent = true; 
    }
  }

  int timeLeft = 0;
  if (isSystemActive) {
      unsigned long elapsed = millis() - lastMotionTime;
      if (elapsed < stayOnTime) {
          timeLeft = (stayOnTime - elapsed) / 1000;
      } else {
          isSystemActive = false; 
          notificationSent = false;
      }
  }
  Blynk.virtualWrite(V7, timeLeft); 

  // 2. SMART LIGHT LOGIC
  if ((isSystemActive && isDark) || physicalLightOn) {
      digitalWrite(lightPin, LOW); 
      if (!physicalLightOn) { 
        Blynk.virtualWrite(V0, 1);   
        Blynk.virtualWrite(V8, 255); 
        Blynk.virtualWrite(V9, "💡 Auto Light ON");
      } else {
        Blynk.virtualWrite(V9, "👆 Manual Light ON");
      }
  } else {
      digitalWrite(lightPin, HIGH); 
      Blynk.virtualWrite(V0, 0); 
      Blynk.virtualWrite(V8, 0);
      if (isSystemActive) Blynk.virtualWrite(V9, "☀️ Daylight Saved");
      else Blynk.virtualWrite(V9, "💤 System Sleep");
  }

  // 3. SMART FAN LOGIC
  if ((isSystemActive && !isnan(temp) && temp > TEMP_THRESHOLD) || manualFanState == LOW || physicalFanOn) {
      digitalWrite(fanPin, LOW); 
  } else {
      digitalWrite(fanPin, HIGH); 
  }

  // 4. THE LCD DISPLAY UPDATE
  lcd.setCursor(0, 0);
  lcd.print("T:"); 
  if(isnan(temp)) lcd.print("0 "); else lcd.print((int)temp);
  lcd.print("C  ");
  lcd.print("Timer:"); lcd.print(timeLeft); lcd.print("s   ");
  
  lcd.setCursor(0, 1);
  if (isSystemActive) {
      if (physicalLightOn || isDark) {
          lcd.print("Light: ON       ");
      } else {
          lcd.print("Daylight Saved  ");
      }
  } else {
      lcd.print("System Sleep... ");
  }
}