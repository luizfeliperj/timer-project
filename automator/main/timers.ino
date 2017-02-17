#undef __FILENAME__
#define __FILENAME__ "timers"
static int8_t lastState = MODEINVALID;
static BlinkTask led = BlinkTask(PIN_LED, LEDBLINKINTERVAL, LEDBLINKINTERVAL, LEDBLINKTIMES);

/* Reinicia o watchdog */
void watchdogger ( Task *task )
{
  char buffer[25];
  time_t tNow = now();
  uint32_t uptime = millis()/MSECS_PER_SEC;
  uint16_t days = uptime/(24L*60L*60L);
  uint16_t hours = uptime%(24L*60L*60L);

  if (lastState == MODEON)
    led.start();

  tNow += ehHorarioDeVerao(tNow, month(tNow), year(tNow)) * SECS_PER_HOUR;

  lcd.setCursor (0,0);
  snprintf_P (buffer, sizeof(buffer)-1, PSTR("%02d/%02d/%02d %01dd%02d:%02d"), day(tNow), month(tNow), tmYearToY2k(CalendarYrToTm(year(tNow))), days, (hours / 3600), ((hours % 3600) / 60));
  lcd.print(buffer);

  lcd.setCursor (0,1);
  snprintf_P (buffer, sizeof(buffer)-1, PSTR("%02d:%02d:%02d   %05d"), hour(tNow), minute(tNow), second(tNow), boot_count);
  lcd.print(buffer);

  wdt_reset();
}

/* Sincroniza a libc com o RTC */
void timerlet ( Task *task )
{
  time_t rtc = getTimeFromRTC();
  time_t source = getDateFromSource();

  debug_print(PSTR("Running timerlet()..."));
  debug_print(PSTR("source: %ld rtc: %ld"), source, rtc);

  if ( rtc < (source - SECS_PER_DAY) )
  {
    setTime(source);
    debug_print(PSTR("rtc inconsist, %ld > %ld. Get source time"), source, rtc);
  } else {
    setTime(rtc);
  }

  new RTCSyncer (task, TIMESAMPLEMSEC);
  doDrift(task, now(), TIMESYNCING);
  return;
}

/* housekeeping, faz o que tem que fazer */
void tasklet ( Task *task )
{
  debug_print(PSTR("Running tasklet()..."));

  time_t tNow = now();
  const int Year = year(tNow);
  const int Month = month(tNow);

  tNow += ehHorarioDeVerao(tNow, Month, Year) * SECS_PER_HOUR;

  const int Hour = hour(tNow);
  const int Minute = minute(tNow);

  const byte Flag = pgm_read_byte (&(timertable[Hour][Minute/TIMERTABLESPLITMIN]));

  if (lastState == MODEINVALID)
  {
    if (Flag == MODEOFF) {
      new Relay(PIN_RELAY2, PRESSBUTTONTIME);
      debug_print(PSTR("Making sure mode is off"));
    }

    lastState = MODEOFF;
    debug_print(PSTR("Last mode was invalid, waiting energy to settle"));
    return;
  }

  debug_print(PSTR("Date: %02d/%04d, Flag: %d, lastState: %d"), Month, Year, Flag, lastState);

  if (lastState != Flag) {
    lastState = Flag;
    switch (Flag) {
      case 1: // Ligar
        lcd.setBacklight(HIGH);
        new Relay(PIN_RELAY1, PRESSBUTTONTIME);
        break;;

      default:
      case 0: // Desligar
        lcd.setBacklight(LOW);
        new Relay(PIN_RELAY2, PRESSBUTTONTIME);
        break;;
    }
  }

  doDrift (task, tNow, POOLINGTIME);
  return;
}

/* Le comandos enviados pela serial regularmente a cada segundo */
void serialtask ( Task *t )
{
  
  time_t tNow = now();
  char buffer[DATETIMESTRINGLEN + 1];
  int uptime = millis()/MSECS_PER_SEC;
  int ano, mes, dia, hora, minuto, segundo;
  SerialTask *task = static_cast<SerialTask*>(t);

  *buffer = Serial.peek();
  if (*buffer == -1) {
    delete task;
    return;
  }

  switch (*buffer) {
#ifdef ENABLE_DEBUG
    case 'd':
    case 'D':
      Serial.read();
      debugenabled = (debugenabled + 1) & 0x1;
      info_print(PSTR("Toggle debugging [%d]"), debugenabled);
      delete task;
      return;
      break;;
#endif /* ENABLE_DEBUG */

    case 'a':
    case 'A':
      Serial.read();
      info_print(PSTR("Compilado por Luiz Felipe Silva"));
      info_print(PSTR("Em " __TIMESTAMP__ ));
      info_print(PSTR("Uptime %d %02d:%02d:%02d"),
          (int)((uptime / SECS_PER_DAY)),
          (int)((uptime % SECS_PER_DAY) / SECS_PER_HOUR),
          (int)((uptime % SECS_PER_HOUR) / SECS_PER_MIN),
          (int)(uptime % 60) );
      info_print(PSTR("Boot count: %d"), get_next_count());
      delete task;
      return;
      break;;

    case 'o':
    case 'O':
      Serial.read();
      new Relay(PIN_RELAY1, PRESSBUTTONTIME);
      info_print(PSTR("Force MODEON"));
      delete task;
      return;
      break;;

    case 'f':
    case 'F':
      Serial.read();
      new Relay(PIN_RELAY2, PRESSBUTTONTIME);
      info_print(PSTR("Force MODEOFF"));
      delete task;
      return;
      break;;

    default:
        if (Serial.readBytes(buffer, DATETIMESTRINGLEN) != DATETIMESTRINGLEN)
        {
          printcurrentdate(tNow);
          delete task;
          return;
        }
        break;;
  }

  buffer[DATETIMESTRINGLEN] = 0;
  if ( 6 != sscanf_P(buffer, PSTR("%04d%02d%02d%02d%02d%02d"), &ano, &mes, &dia, &hora, &minuto, &segundo) )
  {
    printcurrentdate(tNow);
    delete task;
    return;
  }

  info_print(PSTR("*** Setting date to %02d/%02d/%04d %02d:%02d:%02d ***"), dia, mes, ano, hora, minuto, segundo);

  TimeElements T = {(uint8_t)segundo, (uint8_t)minuto, (uint8_t)hora, (uint8_t)0, (uint8_t)dia, (uint8_t)mes, (uint8_t)CalendarYrToTm(ano)};
  tNow = makeTime(T);

  debug_print(PSTR("Converted time is %ld"), tNow);

  tNow -= ehHorarioDeVerao(tNow, month(tNow), year(tNow)) * SECS_PER_HOUR;

  setTime(tNow);
  setDS3231time (second(tNow), minute(tNow), hour(tNow), weekday(tNow), day(tNow), month(tNow), year(tNow) - Y2KMARKFIX);

  info_print(PSTR("!!! Ok !!!"));

  delete task;
  return;
}
