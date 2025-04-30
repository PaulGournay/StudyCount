#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "AiEsp32RotaryEncoder.h"
#include <Arduino.h>

#define ROTARY_DT_PIN 34
#define ROTARY_CLK_PIN 35
#define ROTARY_SW_PIN 19
#define ROTARY_VCC_PIN -1
#define ROTARY_STEPS 4

/*GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(5, 4, 2, 15));*/
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ 5, /*DC=*/ 4, /*RES=*/ 2, /*BUSY=*/ 15)); // DEPG0213BN 122x250, SSD1680
AiEsp32RotaryEncoder rotaryEncoder(ROTARY_DT_PIN, ROTARY_CLK_PIN, ROTARY_SW_PIN, ROTARY_VCC_PIN, ROTARY_STEPS);

bool isTimerStarted = false;
int selectedMinutes = 25;
unsigned long lastEncoderUpdate = 0;
const unsigned long encoderDebounce = 100; // ms

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}
void drawProgressBar(float progress) {
  const int barWidth = display.width() - 40;
  const int barHeight = 20;
  const int x = 20;
  const int y = display.height() - barHeight - 10;

  int fillWidth = (int)(barWidth * progress);

  display.setPartialWindow(x, y, barWidth, barHeight);
  display.firstPage();
  do {
    display.fillRect(x, y, barWidth, barHeight, GxEPD_WHITE);         // clear area
    display.drawRect(x, y, barWidth, barHeight, GxEPD_BLACK);         // border
    display.fillRect(x + 2, y + 2, fillWidth - 4, barHeight - 4, GxEPD_BLACK); // fill
  } while (display.nextPage());
}
void showCenteredText(const char* text) {
    display.setRotation(1);
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(GxEPD_BLACK);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int textX = (display.width() - w) / 2 - x1;
    int textY = display.height() / 2 + h / 2 - 5;

    // Calculate text box area
    int boxMargin = 10;
    int boxY = max(0, textY - h - boxMargin);
    int boxH = h + 2 * boxMargin;

    // Clamp box to stay above progress bar area (e.g., last 30px)
    int progressBarThreshold = display.height() - 30;
    if (boxY + boxH > progressBarThreshold) {
        boxH = progressBarThreshold - boxY;
    }
    

    display.setPartialWindow(0, boxY, display.width(), boxH);

    display.firstPage();
    do {
        display.fillRect(0, boxY, display.width(), boxH, GxEPD_WHITE);
        display.setCursor(textX, textY);
        display.print(text);
    } while (display.nextPage());
}

void showSplashScreen() {
  display.setRotation(1);
  display.setFont(&FreeMonoBold24pt7b);
  display.setTextColor(GxEPD_BLACK);

  const char* title = "Studycount";
  const char* subtitle = "by Studycount Team";

  // Get bounds for title
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  int titleX = (display.width() - w) / 2 - x1;
  int titleY = display.height() / 2 - 10;

  // Set smaller font for subtitle
  display.setFont(NULL); // default font (small)
  int16_t x2, y2;
  uint16_t w2, h2;
  display.getTextBounds(subtitle, 0, 0, &x2, &y2, &w2, &h2);
  int subtitleX = (display.width() - w2) / 2 - x2;
  int subtitleY = titleY + 30;

  // Set window to full screen
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold24pt7b);
    display.setCursor(titleX, titleY);
    display.print(title);
    display.setFont(NULL);
    display.setCursor(subtitleX, subtitleY);
    display.print(subtitle);
  } while (display.nextPage());

  delay(5000); // Show for 5 seconds
}

void pomodoroCountdown(int minutes) {
  unsigned long totalMillis = (unsigned long)minutes * 60UL * 1000UL;
  unsigned long startMillis = millis();
  unsigned long lastUpdate = 0;

  while (millis() - startMillis < totalMillis) {
    unsigned long elapsedMillis = millis() - startMillis;

    // Only update once per second
    if (elapsedMillis - lastUpdate >= 1000) {
      lastUpdate = elapsedMillis;

      unsigned long remainingMillis = totalMillis - elapsedMillis;
      int remainingSeconds = remainingMillis / 1000;
      int m = remainingSeconds / 60;
      int s = remainingSeconds % 60;

      char buffer[6];
      sprintf(buffer, "%02d:%02d", m, s);
      showCenteredText(buffer);

      float progress = (float)elapsedMillis / totalMillis;
      drawProgressBar(progress);
    }

    delay(10); // Small delay to prevent tight loop
  }

  showCenteredText("Break!");
  drawProgressBar(1.0); // full bar
  delay(5000);
}


void setup() {
  Serial.begin(115200);

  // Setup display
  display.init(115200, true, 50, false);
  display.setRotation(1);

  // Show splash screen
  showSplashScreen();

  // Setup rotary
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(1, 120, false);
  rotaryEncoder.setAcceleration(5);

  // Show initial timer
  char buffer[6];
  sprintf(buffer, "%02d:00", selectedMinutes);
  showCenteredText(buffer);
}

void loop() {
  if (rotaryEncoder.encoderChanged() && !isTimerStarted && 
      (millis() - lastEncoderUpdate > encoderDebounce)) {
    lastEncoderUpdate = millis();
    selectedMinutes = rotaryEncoder.readEncoder();
    char buffer[6];
    sprintf(buffer, "%02d:00", selectedMinutes);
    showCenteredText(buffer);
  }

  if (rotaryEncoder.isEncoderButtonClicked() && !isTimerStarted) {
    isTimerStarted = true;
    pomodoroCountdown(selectedMinutes);
    isTimerStarted = false;
    rotaryEncoder.setEncoderValue(selectedMinutes);
  }

  delay(50);
}
