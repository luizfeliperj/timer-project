#undef __FILENAME__
#define __FILENAME__ "main"
#include <Arduino.h>
#include <Wire.h>
#include <TimeLib.h>
#include <SoftTimer.h>
#include <BlinkTask.h>
#include <DelayRun.h>
#include <stdint.h>
#include <EEPROM.h>
#include <LCD.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <LiquidCrystal_I2C.h>

/* DEBUG FLAGS */
#define ENABLE_DEBUG MODEOFF

#define MODEON             1
#define MODEOFF            0
#define MODEINVALID        -1
#define PIN_LED            2
#define PIN_RELAY1         17
#define PIN_RELAY2         16
#define MONTHSPERAYEAR     12
#define MAXSTRINGBUFFER    64
#define BAUDRATE           9600L
#define Y2KMARKFIX         2000L
#define MSECS_PER_SEC      1000UL
#define TIMESYNCING        (60UL * 60UL * MSECS_PER_SEC)
#define TIMESAMPLES        3
#define TIMESAMPLEMSEC     1000UL
#define POOLINGTIME        (60UL * MSECS_PER_SEC)
#define PRESSBUTTONTIME    ( 2UL * MSECS_PER_SEC)
#define PRESSBUTTONTRIES   3
#define SERIALTIMEOUT      100UL
#define LEDBLINKTIMES      3
#define LEDBLINKINTERVAL   100
#define PATHSEPARATOR      '/'
#define TIMERTABLESPLITMIN 5L
#define EEPROMCELLSTOUSE   128
#define DS3231_I2C_ADDRESS 0x68
#define DATETIMESTRINGLEN  sizeof("YYYYMMDDHHMMSS") - 1
#define BACKLIGHT_PIN      3
#define LCD_I2C_ADDRESS    0x3F

#ifdef ENABLE_DEBUG
uint8_t debugenabled = ENABLE_DEBUG;
#define debug_print(fmt, ...) debug_print_hlp(debugenabled, F(__FILENAME__), __LINE__, fmt, ##__VA_ARGS__);
#else
#define debug_print(fmt, ...) 0;
#endif
#define info_print(fmt, ...) debug_print_hlp(true, F(__FILENAME__), __LINE__, fmt, ##__VA_ARGS__);

struct {
  time_t inicio, fim;
  uint16_t year;
} cacheEhHorarioDeVerao;

uint16_t boot_count;
uint16_t get_next_count(const uint8_t);
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 2, 1, 0, 4, 5, 6, 7);

void setup() {
  // put your setup code here, to run once:
  wdt_disable();

  digitalWrite (PIN_RELAY1, HIGH);
  pinMode(PIN_RELAY1, OUTPUT);

  digitalWrite (PIN_RELAY2, HIGH);
  pinMode(PIN_RELAY2, OUTPUT);

  Serial.begin(BAUDRATE);
  Serial.setTimeout(SERIALTIMEOUT);

  Wire.begin();

  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home();

  /* Primeiro ajusta o horario da libc */
  SoftTimer.add(new Task(TIMESYNCING, timerlet));

  /* Segundo, faz o que tem que fazer */
  SoftTimer.add(new Task(POOLINGTIME, tasklet));

  /* Terceiro, gera uma task para zerar o watchdog e atualizar display*/
  SoftTimer.add(new Task(MSECS_PER_SEC, watchdogger));

  memset(&cacheEhHorarioDeVerao, 0, sizeof(cacheEhHorarioDeVerao));

  wdt_enable(WDTO_2S);

  boot_count = get_next_count(1);

  debug_print(PSTR("End of setup process, boot count is %d"), boot_count);
}

void SerialEvent() {
  /* Se houver um SerialEvent, verifica por pedidos na serial */
  SoftTimer.add(new Task(0, serialtask));
}
