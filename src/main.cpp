#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "AiEsp32RotaryEncoder.h"
#include <Arduino.h>

#define ROTARY_DT_PIN 34
#define ROTARY_CLK_PIN 35
#define ROTARY_SW_PIN 19
#define ROTARY_VCC_PIN -1
#define ROTARY_STEPS 4

GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(5, 4, 2, 15));
AiEsp32RotaryEncoder rotaryEncoder(ROTARY_DT_PIN, ROTARY_CLK_PIN, ROTARY_SW_PIN, ROTARY_VCC_PIN, ROTARY_STEPS);

bool isTimerStarted = false;
int selectedMinutes = 25;

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
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
  
    int boxY = max(0, (display.height() - h) / 2 - 20);
    int boxH = min((int)display.height(), (int)h + 40);
    display.setPartialWindow(0, boxY, display.width(), boxH);
  
    display.firstPage();
    do {
      display.fillRect(0, boxY, display.width(), boxH, GxEPD_WHITE);
      display.setCursor(textX, textY);
      display.print(text);
    } while (display.nextPage());
  }

void pomodoroCountdown(int minutes) {
  for (int m = minutes - 1; m >= 0; m--) {
    for (int s = 59; s >= 0; s--) {
      char buffer[6];
      sprintf(buffer, "%02d:%02d", m, s);
      showCenteredText(buffer);
      delay(1000);
    }
  }

  showCenteredText("Break!");
  delay(5000);
}

void setup() {
  Serial.begin(115200);

  // Setup display
  display.init(115200, true, 50, false);
  display.setRotation(1);

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
  if (rotaryEncoder.encoderChanged() && !isTimerStarted) {
    selectedMinutes = rotaryEncoder.readEncoder();
    char buffer[6];
    sprintf(buffer, "%02d:00", selectedMinutes);
    showCenteredText(buffer);
  }

  if (rotaryEncoder.isEncoderButtonClicked() && !isTimerStarted) {
      isTimerStarted = true;
    pomodoroCountdown(selectedMinutes);
    isTimerStarted = false;
    rotaryEncoder.setEncoderValue(selectedMinutes);  // Reset value after countdown
  }

  delay(50);
}
