#include <Arduino.h>
#include <Wire.h>
#include <TimeLib.h>
#include <SoftTimer.h>
#include <BlinkTask.h>
#include <DelayRun.h>
#include <stdint.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

struct {
  int year;
  time_t inicio, fim;
} cacheEhHorarioDeVerao;

uint16_t get_next_count(const uint8_t);

/* DEBUG FLAGS */
#define ENABLE_DEBUG MODEOFF

#define MODEON             1
#define MODEOFF            0
#define MODEINVALID        -1
#define PIN_LED            2
#define PIN_RELAY1         17
#define PIN_RELAY2         16
#define MONTHSPERAYEAR     12
#define MAXSTRINGBUFFER    128
#define BAUDRATE           9600L
#define Y2KMARKFIX         2000L
#define TIMESYNCING        (60 * 60 * 1000L)
#define POOLINGTIME        (30 * 1000L)
#define PRESSBUTTONTIME    ( 2 * 1000L)
#define PRESSBUTTONTRIES   3
#define SERIALTIMEOUT      100L
#define SERIALPOOLING      1000L
#define LEDBLINKTIMES      3
#define LEDBLINKINTERVAL   100
#define PATHSEPARATOR      '/'
#define TIMERTABLESPLITMIN 5L
#define EEPROMCELLSTOUSE   128
#define SECONDS_IN_MINUTE  60
#define DS3231_I2C_ADDRESS 0x68
#define SECONDS_IN_HOUR    (60L * SECONDS_IN_MINUTE)
#define DATETIMESTRINGLEN  sizeof("YYYYMMDDHHMMSS") - 1

#ifdef ENABLE_DEBUG
uint8_t debugenabled = ENABLE_DEBUG;
#define debug_print(fmt, ...) debug_print_hlp(debugenabled, F(__FILE__), __LINE__, fmt, ##__VA_ARGS__);
#else
#define debug_print(fmt, ...) 0;
#endif
#define info_print(fmt, ...) debug_print_hlp(1, F(__FILE__), __LINE__, fmt, ##__VA_ARGS__);

void setup() {
  // put your setup code here, to run once:
  wdt_disable();

  Wire.begin();

  cacheEhHorarioDeVerao.fim = 0;
  cacheEhHorarioDeVerao.year = 0;
  cacheEhHorarioDeVerao.inicio = 0;

  #ifdef ENABLE_DEBUG
  while (!Serial);
  #endif // ENABLE_DEBUG

  if (Serial) {
    Serial.begin(BAUDRATE);
    Serial.setTimeout(SERIALTIMEOUT);
  }

  digitalWrite (PIN_RELAY1, HIGH);
  pinMode( PIN_RELAY1, OUTPUT );

  digitalWrite (PIN_RELAY2, HIGH);
  pinMode( PIN_RELAY2, OUTPUT );

  /* Primeiro ajusta o horario da libc */
  SoftTimer.add (new Task (TIMESYNCING, timerlet));

  /* Segundo, faz o que tem que fazer */
  SoftTimer.add (new Task (POOLINGTIME, tasklet));

  /* Terceiro, verifica por pedidos na serial */
  SoftTimer.add (new Task (SERIALPOOLING, seriallet));

  wdt_enable(WDTO_2S);

  debug_print(PSTR("End of setup process, boot count is %d"), get_next_count(1));
}
