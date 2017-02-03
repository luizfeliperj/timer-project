#undef __FILENAME__
#define __FILENAME__ "timers"
static int8_t lastState = MODEINVALID;
static BlinkTask led = BlinkTask(PIN_LED, LEDBLINKINTERVAL, LEDBLINKINTERVAL, LEDBLINKTIMES);

/* Reinicia o watchdog */
void watchdogger ( Task *me )
{
  wdt_reset();
  debug_print(PSTR("Watchdog reset"));
}

/* Atualiza o display */
void updater ( Task *me )
{
  char buffer[100];
  int uptime = millis()/1000;
  int days = uptime/(24*60*60);
  int hours = uptime%(24*60*60);

  u8g.firstPage();
  do {
    u8g.drawStrP( 0, 20, PSTR("timer-project:"));

    sprintf (buffer, "Up: %02d %02d:%02d:%02d", days, (hours / 3600), ((hours % 3600) / 60), (hours % 60));
    u8g.drawStr( 0, 63, buffer);
  } while( u8g.nextPage() );
}

/* Le comandos enviados pela serial regularmente a cada segundo */
void seriallet ( Task *me )
{
  time_t t = now();
  time_t uptime = millis();
  char buffer[DATETIMESTRINGLEN + 1];
  int ano, mes, dia, hora, minuto, segundo;
  
  if (lastState == MODEON) {
    debug_print(PSTR("Mode is on, blink led"));
    led.start();
  }

  if (!Serial)
    return;

  *buffer = Serial.peek();
  if (*buffer == -1)
    return;
  
  switch (*buffer) {
#ifdef ENABLE_DEBUG
    case 'd':
    case 'D':
      Serial.read();
      debugenabled = (debugenabled + 1) & 0x1;
      info_print(PSTR("Toggle debugging [%d]"), debugenabled);
      return;
#endif /* ENABLE_DEBUG */
    
    case 'a':
    case 'A':
      Serial.read();
      info_print(PSTR("Compilado por Luiz Felipe Silva"));
      info_print(PSTR("Em " __TIMESTAMP__ ));
      info_print(PSTR("Uptime %d %02d:%02d:%02d"),
          (uptime/MSECS_PER_SEC)  / SECS_PER_DAY,
          ((uptime/MSECS_PER_SEC) % SECS_PER_DAY)  / SECS_PER_HOUR,
          ((uptime/MSECS_PER_SEC) % SECS_PER_HOUR) / SECS_PER_MIN,
          (uptime/MSECS_PER_SEC)  % SECS_PER_MIN );
      info_print(PSTR("Boot count: %d"), get_next_count());
      return;

    case 'o':
    case 'O':
      Serial.read();
      new Relay(PRESSBUTTONTIME, PIN_RELAY1);
      info_print(PSTR("Force MODEON"));
      return;

    case 'f':
    case 'F':
      Serial.read();
      new Relay(PRESSBUTTONTIME, PIN_RELAY2);
      info_print(PSTR("Force MODEOFF"));
      return;

    default:
        if (Serial.readBytes(buffer, DATETIMESTRINGLEN) != DATETIMESTRINGLEN)
        {
          printcurrentdate(t);
          return;
        }
  }
  
  buffer[DATETIMESTRINGLEN] = 0;
  if ( 6 != sscanf_P(buffer, PSTR("%04d%02d%02d%02d%02d%02d"), &ano, &mes, &dia, &hora, &minuto, &segundo) )
  {
    printcurrentdate(t);
    return;
  }

  info_print(PSTR("*** Setting date to %02d/%02d/%04d %02d:%02d:%02d ***"), dia, mes, ano, hora, minuto, segundo);

  TimeElements T = {(uint8_t)segundo, (uint8_t)minuto, (uint8_t)hora, (uint8_t)0, (uint8_t)dia, (uint8_t)mes, (uint8_t)CalendarYrToTm(ano)};
  t = makeTime(T);

  debug_print(PSTR("Converted time is %ld"), t);
    
  int EhHorarioDeVerao = ehHorarioDeVerao(t, month(t), year(t));
  if (EhHorarioDeVerao)
   {
    t -= SECS_PER_HOUR;
    debug_print(PSTR("ehHorarioDeVerao returned true"));
   }

  setTime(t);
  setDS3231time (second(t), minute(t), hour(t), weekday(t), day(t), month(t), year(t) - Y2KMARKFIX);

  info_print(PSTR("!!! Ok !!!"));

  SoftTimer.remove(me);
  delete me;

  return;
}

/* Sincroniza a libc com o RTC */
void timerlet ( Task *me )
{
  debug_print(PSTR("Running timerlet()..."));
  
  time_t t = now();
  time_t rtc = getTimeFromRTC();
  time_t source = getDateFromSource();

  debug_print(PSTR("now(): %ld source: %ld rtc: %ld"), t, source, rtc);

  if ( rtc < source  )
  {
    debug_print(PSTR("rtc time is inconsist, %ld > %ld. Using source time"), source, rtc);
    rtc = source;
  }

  new RTCSyncer (TIMESAMPLEMSEC, rtc, me);

  return;
}

/* housekeeping, faz o que tem que fazer */
void tasklet ( Task *me )
{
  debug_print(PSTR("Running tasklet()..."));

  time_t t = now();
  debug_print(PSTR("Now in time_t: %ld"), t);
  
  const int Year = year(t);
  const int Month = month(t);
  debug_print(PSTR("Date: %02d/%04d"), Month, Year);

  const int EhHorarioDeVerao = ehHorarioDeVerao(t, Month, Year);
  debug_print(PSTR("EhHorarioDeVerao: %d"),EhHorarioDeVerao);

  t += EhHorarioDeVerao * SECS_PER_HOUR;

  const int Hour = hour(t);
  const int Minute = minute(t);
  debug_print(PSTR("Hour: %02d:%02d"), Hour, Minute);
  
  const byte Flag = pgm_read_byte (&(timertable[Hour][Minute/TIMERTABLESPLITMIN]));
  debug_print(PSTR("Flag: %d"), Flag);
  
  if (lastState == MODEINVALID)
  {
    if (Flag == MODEOFF) {
      debug_print(PSTR("Making sure mode is off"));
      new Relay(PRESSBUTTONTIME, PIN_RELAY2);
    }
  return;

    debug_print(PSTR("Last mode was invalid, waiting energy to settle"));
    lastState = MODEOFF;

    return;
  }
  
  debug_print(PSTR("lastState: %d"), lastState);
  if (lastState != Flag) {
    lastState = Flag;
    switch (Flag) {
      case 0: // Desligar
        new Relay(PRESSBUTTONTIME, PIN_RELAY2);
        break;;

      case 1: // Ligar
        new Relay(PRESSBUTTONTIME, PIN_RELAY1);
        break;;

      default:
        debug_print(PSTR("Invalid Button"));
        break;;
    }
  }

  unsigned int drift = t % (POOLINGTIME/MSECS_PER_SEC);
  if (drift)
  {
    int period = (POOLINGTIME/MSECS_PER_SEC) - drift;
    new TimerFixer (period * MSECS_PER_SEC, me);
    debug_print(PSTR("tasklet Need to drift %d seconds"), period);
  }

  return;
}
