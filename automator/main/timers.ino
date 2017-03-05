#undef __FILENAME__
#define __FILENAME__ "timers"
uint8_t lastState = MODEINVALID;

/* Reinicia o watchdog */
void watchdogger ( Task *task )
{
  char buffer[25];
  time_t tNow = now();
  uint32_t uptime = millis()/MSECS_PER_SEC;
  uint32_t hours = uptime%(24UL*3600UL);
  uint16_t days = uptime/(24UL*3600UL);

  if (lastState == MODEON)
    digitalWrite (PIN_LED, HIGH);
  else
    digitalWrite (PIN_LED, LOW);

  if (ehHorarioDeVerao(tNow, year(tNow)))
    tNow += SECS_PER_HOUR;

  lcd.setCursor (0,0);
  snprintf_P (buffer, sizeof(buffer)-1, PSTR("%02d/%02d/%02d %01ud%02lu:%02lu"),
    day(tNow), month(tNow), tmYearToY2k(CalendarYrToTm(year(tNow))), days, (hours / 3600UL), ((hours % 3600UL) / 60UL));
  lcd.print(buffer);

  lcd.setCursor (0,1);
  snprintf_P (buffer, sizeof(buffer)-1, PSTR("%02d:%02d:%02d   %05u"), hour(tNow), minute(tNow), second(tNow), boot_count);
  lcd.print(buffer);

  wdt_reset();
}

/* Sincroniza a libc com o RTC */
void timerlet ( Task *task )
{
  time_t rtc = getTimeFromRTC();
  time_t source = getDateFromSource();

  debug_print(PSTR("Running timerlet()..."));

  if ( rtc < (source - SECS_PER_DAY) )
  {
    debug_print(PSTR("source: %ld rtc: %ld"), source, rtc);
    debug_print(PSTR("rtc inconsist, %ld > %ld. Get source time"), source, rtc);
    setTime(source);
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

  if (ehHorarioDeVerao(tNow, Year))
    tNow +=  SECS_PER_HOUR;

  const int Hour = hour(tNow);
  const int Minute = minute(tNow);

  const byte Flag = pgm_read_byte (&(timertable[Hour][Minute/TIMERTABLESPLITMIN]));

  if (lastState == MODEINVALID) {
    if (Flag == MODEOFF) {
      new Relay(PIN_RELAY2, PRESSBUTTONTIME);
      debug_print(PSTR("Making sure mode is off"));
    }

    lastState = MODEOFF;
    debug_print(PSTR("Last mode was invalid, waiting energy to settle"));
    return;
  }

  debug_print(PSTR("Flag: %d, lastState: %d"), Flag, lastState);

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
  uint16_t days;
  uint32_t uptime, hours;
  char buffer[DATETIMESTRINGLEN + 1];
  
  SerialTask *task = static_cast<SerialTask*>(t);

  *buffer = Serial.read();
  if (*buffer == -1) {
    delete task;
    return;
  }

  switch (*buffer) {
#ifdef ENABLE_DEBUG
    case 'd':
    case 'D':
      debugenabled = (debugenabled + 1) & 0x1;
      info_print(PSTR("Toggle debugging [%d]"), debugenabled);
      break;
#endif /* ENABLE_DEBUG */

    case 'a':
    case 'A':
      uptime = millis()/MSECS_PER_SEC;
      days = uptime/(24UL*3600UL);
      hours = uptime%(24UL*3600UL);

      info_print(PSTR("Compilado por Luiz Felipe Silva"));
      info_print(PSTR("Em " __TIMESTAMP__ ));
      info_print(PSTR("Uptime %02u %02lu:%02lu:%02lu"),
          days, (hours / 3600UL), ((hours % 3600UL) / 60UL), ((hours % 3600UL) % 60UL));
      info_print(PSTR("Boot count: %d /sram free: [%d] / millis: [%lu]"), get_next_count(), freeRam(), millis());
      break;

    case 'o':
    case 'O':
      new Relay(PIN_RELAY1, PRESSBUTTONTIME);
      info_print(PSTR("Force MODEON"));
      break;

    case 'f':
    case 'F':
      new Relay(PIN_RELAY2, PRESSBUTTONTIME);
      info_print(PSTR("Force MODEOFF"));
      break;

    default:
      if (Serial.readBytes(&buffer[1], DATETIMESTRINGLEN-1) == DATETIMESTRINGLEN-1) {
        serialSetTime(buffer, now());
        break;
      }

      printcurrentdate(now());
      break;
  }

  delete task;
  return;
}
