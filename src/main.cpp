#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "AiEsp32RotaryEncoder.h"
#include <Arduino.h>
#include "logo.h"

#define ROTARY_DT_PIN 34
#define ROTARY_CLK_PIN 35
#define ROTARY_SW_PIN 19
#define ROTARY_VCC_PIN -1
#define ROTARY_STEPS 2
#define BUZZER_PIN 25

/*
digital encoder:
CLK-->D35
DT-->D34
SW-->D19

e-ink screen:
BUSY-->D15
RES-->D2
D/C-->D4
SC-->D5
SCL-->D18
SDA-->D23 */

/*GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(5, 4, 2, 15));*/
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ 5, /*DC=*/4, /*RES=*/2, /*BUSY=*/15)); // DEPG0213BN 122x250, SSD1680
AiEsp32RotaryEncoder rotaryEncoder(ROTARY_DT_PIN, ROTARY_CLK_PIN, ROTARY_SW_PIN, ROTARY_VCC_PIN, ROTARY_STEPS);

bool isTimerStarted = false;
int selectedMinutes = 25;
unsigned long lastEncoderUpdate = 0;
const unsigned long encoderDebounce = 100; // ms

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}
void drawProgressBar(float progress)
{
  const int barWidth = display.width() - 40;
  const int barHeight = 20;
  const int x = 20;
  const int y = display.height() - barHeight - 10;

  int fillWidth = (int)(barWidth * progress);

  display.setPartialWindow(x, y, barWidth, barHeight);
  display.firstPage();
  do
  {
    display.fillRect(x, y, barWidth, barHeight, GxEPD_WHITE);                  // clear area
    display.drawRect(x, y, barWidth, barHeight, GxEPD_BLACK);                  // border
    display.fillRect(x + 2, y + 2, fillWidth - 4, barHeight - 4, GxEPD_BLACK); // fill
  } while (display.nextPage());
}
void showCenteredText(const char *text)
{
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
  if (boxY + boxH > progressBarThreshold)
  {
    boxH = progressBarThreshold - boxY;
  }

  display.setPartialWindow(0, boxY, display.width(), boxH);

  display.firstPage();
  do
  {
    display.fillRect(0, boxY, display.width(), boxH, GxEPD_WHITE);
    display.setCursor(textX, textY);
    display.print(text);
  } while (display.nextPage());
}

void showSplashScreen()
{
  display.setRotation(1);
  display.setFullWindow();
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold18pt7b);

  const char *title = "Studycount";
  const char *subtitle = "by Studycount Team";

  // Get text bounds
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

  // Calculate true center position
  int textX = (display.width() - w) / 2 - x1;
  int textY = (display.height() - h) / 2 - y1;
  display.setFont(NULL);
  int subtitleX = (display.width() - w) / 2 - x1;
  int subtitleY = textY + h + 20;
  // Render splash
  display.firstPage();
  do
  {
    display.setFont(&FreeMonoBold18pt7b);
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(textX, textY);
    display.print(title);
    display.setFont(NULL);
    display.setCursor(subtitleX, subtitleY);
    display.print(subtitle);
  } while (display.nextPage());
  delay(5000);
}

/*
void showSplashScreen() {
  display.setRotation(1);
  display.setFullWindow();

  display.firstPage();
  do {
    display.drawBitmap((display.width() - logo_width) / 2, 10, logo_bits, logo_width, logo_height, GxEPD_BLACK);
  } while (display.nextPage());
  delay(5000);
}*/
int showMenu()
{
  int menuOption = 0;
  const char *options[] = {"Pause for 5 min", "Choose another timer"};
  const int numOptions = 2;

  rotaryEncoder.setBoundaries(0, numOptions - 1, true); // navigation entre 0 et 1
  rotaryEncoder.setEncoderValue(0);

  while (true)
  {
    if (rotaryEncoder.encoderChanged())
    {
      menuOption = rotaryEncoder.readEncoder();

      display.setRotation(1);
      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        for (int i = 0; i < numOptions; i++)
        {
          int y = 40 + i * 30;
          if (i == menuOption)
          {
            display.fillRect(10, y - 15, display.width() - 20, 25, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
          }
          else
          {
            display.setTextColor(GxEPD_BLACK);
          }
          display.setCursor(20, y);
          display.setFont(&FreeSans9pt7b); // petite police
          display.print(options[i]);
        }
      } while (display.nextPage());
    }

    if (rotaryEncoder.isEncoderButtonClicked())
    {
      return menuOption;
    }

    delay(50);
  }
}

void pomodoroCountdown(int minutes)
{
  unsigned long totalMillis = (unsigned long)minutes * 60UL * 1000UL;
  unsigned long startMillis = millis();
  unsigned long lastUpdate = 0;

  while (millis() - startMillis < totalMillis)
  {
    unsigned long elapsedMillis = millis() - startMillis;

    // Only update once per second
    if (elapsedMillis - lastUpdate >= 1000)
    {
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

  for (int i = 0; i < 5; i++)
  {
    tone(BUZZER_PIN, 2000); // 2kHz tone
    delay(100);
    noTone(BUZZER_PIN);
    delay(100);
  }

  showCenteredText("Break!");
  drawProgressBar(1.0);
  delay(3000);

  int choice = showMenu();
  if (choice == 0)
  {
    pomodoroCountdown(5); // Pause de 5 minutes
  }
  else
  {
    // Retour au menu principal (affichage du temps sélectionné)
    char buffer[6];
    sprintf(buffer, "%02d:00", selectedMinutes);
    showCenteredText(buffer);
    rotaryEncoder.setBoundaries(1, 120, false);
  }
}

//****************************************SETUP**************************************** */
void setup()
{
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // make sure it's off initially

  // Setup display
  display.init(115200, true, 50, false);
  display.setRotation(1);

  // Show splash screen
  showSplashScreen();

  // Clear full screen after splash
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());

  // Enable pullups on encoder pins (HW-040 requires them)
  pinMode(ROTARY_DT_PIN, INPUT_PULLUP);
  pinMode(ROTARY_CLK_PIN, INPUT_PULLUP);
  pinMode(ROTARY_SW_PIN, INPUT_PULLUP);

  // Setup rotary
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  // Optional: reverse direction if it turns backward
  // rotaryEncoder.setDirection(REVERSE);

  rotaryEncoder.setBoundaries(1, 120, false);
  rotaryEncoder.setAcceleration(5);

  // Show initial timer
  char buffer[6];
  sprintf(buffer, "%02d:00", selectedMinutes);
  showCenteredText(buffer);
}
//****************************************LOOP**************************************** */
void loop()
{
  if (rotaryEncoder.encoderChanged() && !isTimerStarted &&
      (millis() - lastEncoderUpdate > encoderDebounce))
  {
    lastEncoderUpdate = millis();
    selectedMinutes = rotaryEncoder.readEncoder();
    char buffer[6];
    sprintf(buffer, "%02d:00", selectedMinutes);
    showCenteredText(buffer);
  }

  if (rotaryEncoder.isEncoderButtonClicked() && !isTimerStarted)
  {
    isTimerStarted = true;
    pomodoroCountdown(selectedMinutes);
    isTimerStarted = false;
    rotaryEncoder.setEncoderValue(selectedMinutes);
  }

  delay(50);
}
