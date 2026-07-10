/*
 * VAULT — RFID Combination Lock with Temperature-Triggered Hold-Open
 *  - Scan an RFID card to begin. Enter a 4-digit code using the two buttons
 *    (cycle changes the digit, confirm locks it in). Correct code opens the
 *    servo latch. The vault then stays open until the DHT11 senses the
 *    temperature rise 5C above what it was at the moment of unlock, at
 *    which point it automatically re-locks.
 *  - A sudden temperature/humidity jump while locked/entering code is
 *    treated as tampering and triggers an escalating lockout, same as
 *    repeated wrong codes.
 *  - The passive buzzer plays a distinct low buzz on any failure (wrong
 *    code or tamper lockout), separate from the short beeps used for
 *    normal button presses / successful unlock.
 *  - The LCD's bottom-right corner always shows the live temperature.
 *
 * TESTING CODES (Serial Monitor, 9600 baud):
 *  - Type "scan" to simulate an RFID card scan.
 *  - Type "temp <value>" (e.g. "temp 24.5") to override the live temp
 *    reading. 
 * CODE IS 1337
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <DHT.h>
#include <Servo.h>

// ---------- Pin map ----------
#define RFID_SS_PIN   10
#define RFID_RST_PIN  9

#define RTC_CLK_PIN   3
#define RTC_DAT_PIN   2
#define RTC_RST_PIN   6

#define DHT_PIN       A0
#define SERVO_PIN     8
#define BTN_CYCLE     A1
#define BTN_CONFIRM   A2
#define BUZZER_PIN    A3

// ---------- Objects ----------
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);   

ThreeWire rtcWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN); 
RtcDS1302<ThreeWire> rtc(rtcWire);

DHT dht(DHT_PIN, DHT11);
Servo latch;

// ---------- State machine ----------
enum State { LOCKED, AUTHENTICATING, ENTERING_CODE, UNLOCKING, UNLOCKED, LOCKOUT };
State state = LOCKED;

// ---------- Secret code ----------
const int CODE_LENGTH = 4;
int secretCode[CODE_LENGTH] = {1, 3, 3, 7};
int enteredCode[CODE_LENGTH];
int codeIndex = 0;
int currentDigit = 0;

// ---------- Timing (all non-blocking) ----------
unsigned long lastDebounceCycle = 0;
unsigned long lastDebounceConfirm = 0;
const unsigned long DEBOUNCE_MS = 180;

unsigned long confirmPressStart = 0;
bool confirmHeld = false;
const unsigned long LONG_PRESS_MS = 2000;

unsigned long stateEnteredAt = 0;
unsigned long lastTamperCheck = 0;
const unsigned long TAMPER_CHECK_INTERVAL = 2000;

unsigned long lockoutUntil = 0;
int failedAttempts = 0;

// ---------- Tamper baseline ----------
float baselineTemp = NAN;
float baselineHumidity = NAN;
const float TAMPER_TEMP_DELTA = 4.0;
const float TAMPER_HUMIDITY_DELTA = 15.0;

// ---------- Live temperature (shown on screen + used for unlock-hold logic) ----------
float currentTemp = NAN;
float tempAtUnlock = NAN;
const float UNLOCK_HOLD_TEMP_RISE = 5.0; // vault re-locks once temp rises this many degrees above tempAtUnlock

void setup() {
  Serial.begin(9600);

  pinMode(BTN_CYCLE, INPUT_PULLUP);
  pinMode(BTN_CONFIRM, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();

  Wire.begin();
  lcd.init();
  lcd.backlight();

  rtc.Begin();
  if (!rtc.IsDateTimeValid()) {
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    rtc.SetDateTime(compiled);
  }
  if (rtc.GetIsWriteProtected()) {
    rtc.SetIsWriteProtected(false);
  }
  if (!rtc.GetIsRunning()) {
    rtc.SetIsRunning(true);
  }

  dht.begin();
  latch.attach(SERVO_PIN);
  latch.write(0); // locked position

  delay(1500); 
  baselineTemp = dht.readTemperature();
  baselineHumidity = dht.readHumidity();
  currentTemp = baselineTemp;

  showLockedScreen();
}

void loop() {
  unsigned long now = millis();

  checkTamper(now);
  checkLongPressReset(now);
  handleSerialTestCommands();
  drawTempCorner();

  switch (state) {
    case LOCKED:
      handleLocked();
      break;
    case AUTHENTICATING:
      handleAuthenticating(now);
      break;
    case ENTERING_CODE:
      handleEnteringCode(now);
      break;
    case UNLOCKING:
      handleUnlocking(now);
      break;
    case UNLOCKED:
      handleUnlocked(now);
      break;
    case LOCKOUT:
      handleLockout(now);
      break;
  }
}

// ---------------- Serial testing commands ----------------


void handleSerialTestCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.equalsIgnoreCase("scan") && state == LOCKED) {
    Serial.println("[TEST] Simulated card scan");
    beep(1);
    changeState(AUTHENTICATING);
  } else if (cmd.startsWith("temp ")) {
    float simTemp = cmd.substring(5).toFloat();
    currentTemp = simTemp;
    if (isnan(baselineTemp)) baselineTemp = simTemp;
    Serial.print("[TEST] Simulated temp set to: ");
    Serial.println(currentTemp);
  }
}

// ---------------- State handlers ----------------

void handleLocked() {
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.println("Card detected!");
    beep(1);
    rfid.PICC_HaltA();
    changeState(AUTHENTICATING);
  }
}

void handleAuthenticating(unsigned long now) {
  if (now - stateEnteredAt > 800) {
    codeIndex = 0;
    currentDigit = 0;
    changeState(ENTERING_CODE);
  }
}

void handleEnteringCode(unsigned long now) {
  lcd.setCursor(0, 1);
  lcd.print("D");
  lcd.print(codeIndex + 1);
  lcd.print(":");
  lcd.print(currentDigit);
  lcd.print("      "); 

  if (buttonPressed(BTN_CYCLE, lastDebounceCycle, now)) {
    currentDigit = (currentDigit + 1) % 10;
  }

  // short-press confirm = confirm digit (long-press elsewhere = reset)
  if (digitalRead(BTN_CONFIRM) == LOW) {
    if (!confirmHeld && (now - lastDebounceConfirm) > DEBOUNCE_MS) {
      lastDebounceConfirm = now;
      enteredCode[codeIndex] = currentDigit;
      codeIndex++;
      currentDigit = 0;
      beep(1);

      if (codeIndex >= CODE_LENGTH) {
        if (checkCode()) {
          changeState(UNLOCKING);
        } else {
          registerFailedAttempt(now);
        }
      }
    }
  }
}

void handleUnlocking(unsigned long now) {
  latch.write(90); // open position
  logUnlockEvent();
  beep(3);
  tempAtUnlock = currentTemp; // vault stays open until temp rises 5C above this
  changeState(UNLOCKED);
}

void handleUnlocked(unsigned long now) {
  lcd.setCursor(0, 1);
  lcd.print("Open   "); 

  bool tempRoseEnough = !isnan(currentTemp) && !isnan(tempAtUnlock) &&
                         (currentTemp - tempAtUnlock >= UNLOCK_HOLD_TEMP_RISE);

  if (tempRoseEnough) {
    latch.write(0);
    failedAttempts = 0;
    changeState(LOCKED);
  }
}

void handleLockout(unsigned long now) {
  unsigned long remaining = (lockoutUntil > now) ? (lockoutUntil - now) / 1000 : 0;
  lcd.setCursor(0, 1);
  lcd.print("Wait ");
  lcd.print(remaining);
  lcd.print("s   "); 

  if (now >= lockoutUntil) {
    changeState(LOCKED);
  }
}

// ---------------- Reset via long-press ----------------

void checkLongPressReset(unsigned long now) {
  if (digitalRead(BTN_CONFIRM) == LOW) {
    if (confirmPressStart == 0) {
      confirmPressStart = now;
    } else if (!confirmHeld && (now - confirmPressStart) > LONG_PRESS_MS) {
      confirmHeld = true;
      beep(2);
      changeState(LOCKED);
    }
  } else {
    confirmPressStart = 0;
    confirmHeld = false;
  }
}

// ---------------- Helpers ----------------

bool checkCode() {
  for (int i = 0; i < CODE_LENGTH; i++) {
    if (enteredCode[i] != secretCode[i]) return false;
  }
  return true;
}

void registerFailedAttempt(unsigned long now) {
  failedAttempts++;
  failBeep();
  unsigned long lockoutSeconds = 5UL << (failedAttempts - 1 > 4 ? 4 : failedAttempts - 1);
  lockoutUntil = now + lockoutSeconds * 1000UL;
  changeState(LOCKOUT);
}

void changeState(State newState) {
  state = newState;
  stateEnteredAt = millis();

  switch (newState) {
    case LOCKED:
      showLockedScreen();
      break;
    case AUTHENTICATING:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card detected...");
      break;
    case ENTERING_CODE:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter code:");
      break;
    case UNLOCKING:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Unlocking...");
      break;
    case UNLOCKED:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("VAULT OPEN");
      break;
    case LOCKOUT:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LOCKED OUT");
      break;
  }
}

bool buttonPressed(int pin, unsigned long &lastDebounce, unsigned long now) {
  if (digitalRead(pin) == LOW && (now - lastDebounce) > DEBOUNCE_MS) {
    lastDebounce = now;
    return true;
  }
  return false;
}

void beep(int times) {
  tone(BUZZER_PIN, 1200, 80 * times);
}

void failBeep() {
  tone(BUZZER_PIN, 300, 500); 
}

void showLockedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VAULT: Scan card");
}

void logUnlockEvent() {
  RtcDateTime t = rtc.GetDateTime();
  Serial.print("Unlock at ");
  Serial.print(t.Year()); Serial.print('-');
  Serial.print(t.Month()); Serial.print('-');
  Serial.print(t.Day()); Serial.print(' ');
  Serial.print(t.Hour()); Serial.print(':');
  Serial.print(t.Minute()); Serial.print(':');
  Serial.println(t.Second());
}

void checkTamper(unsigned long now) {
  if (now - lastTamperCheck < TAMPER_CHECK_INTERVAL) return;
  lastTamperCheck = now;

  
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) return; 

  currentTemp = temp;

  bool tempJump = !isnan(baselineTemp) && fabs(temp - baselineTemp) > TAMPER_TEMP_DELTA;
  bool humJump = !isnan(baselineHumidity) && fabs(hum - baselineHumidity) > TAMPER_HUMIDITY_DELTA;

  if ((tempJump || humJump) && state != LOCKOUT) {
    failBeep();
    failedAttempts = max(failedAttempts, 2);
    lockoutUntil = now + 15000UL;
    changeState(LOCKOUT);
  }
}

// ---------------- Temperature readout (bottom-right of LCD) ----------------


void drawTempCorner() {
  lcd.setCursor(11, 1);
  if (isnan(currentTemp)) {
    lcd.print(" --C ");
  } else {
    char buf[6];
    dtostrf(currentTemp, 4, 1, buf); // e.g. "24.5"
    lcd.print(buf);
    lcd.print("C");
  }
}
