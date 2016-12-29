static int8_t lastState = MODEINVALID;
static Relay relayOn(PRESSBUTTONTIME, PIN_RELAY1);
static Relay relayOff(PRESSBUTTONTIME, PIN_RELAY2);
static BlinkTask led = BlinkTask(PIN_LED, LEDBLINKINTERVAL, LEDBLINKINTERVAL, LEDBLINKTIMES);

/* Le comandos enviados pela serial regularmente a cada segundo */
void seriallet ( Task *me )
{
  time_t t = now();
  char buffer[DATETIMESTRINGLEN + 1];
  int ano, mes, dia, hora, minuto, segundo;

  wdt_reset();

  if (lastState == MODEON)
    led.start();

  if (!Serial)
    return;

  *buffer = Serial.peek();
  if (*buffer == -1)
    return;

  switch (*buffer) {
#ifdef ENABLE_DEBUG
    case 'D':
      Serial.read();
      debugenabled = (debugenabled + 1) & 0x1;
      break;
#endif /* ENABLE_DEBUG */

    case 'O':
      Serial.read();
      relayOn.pressButton();
      return;

    case 'F':
      Serial.read();
      relayOff.pressButton();
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

  debug_print(PSTR("*** Setting date to %02d/%02d/%04d %02d:%02d:%02d ***"), dia, mes, ano, hora, minuto, segundo);

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

  Serial.println(F("!!! Ok !!!"));

  return;
}

/* Sincroniza a libc com o RTC */
void timerlet ( Task *me )
{
  time_t t, rtc;
  
  t = now();
  rtc = getTimeFromRTC();

  debug_print(PSTR("time_t: %ld rtc: %ld"), t, rtc);

  if (t == 0L)
    setDateFromSource(&t);

  if (t != rtc)
  {
    t = rtc;
    setTime(rtc);
  }

  unsigned int needdrift = t % (TIMESYNCING/1000UL);
  if (needdrift)
  {
    int period = (TIMESYNCING/1000UL) - needdrift;

    TimerFixer *t = new TimerFixer (period * 1000UL, me);
    t->startDelayed();
    
    debug_print(PSTR("timerlet Need to drift %d seconds"), period);
  }

  debug_print(PSTR("Time from RTC is %ld"), rtc);
  debug_print(PSTR("Time from libc is %ld"), t);
  debug_print(PSTR("drift is %ld"), t - rtc);
  
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
    debug_print(PSTR("Last mode was invalid, waiting energy to settle"));

    lastState = MODEOFF;

    if (Flag == MODEOFF)
      relayOff.pressButton();

    return;
  }
  
  debug_print(PSTR("lastState: %d"), lastState);
  if (lastState != Flag) {
    lastState = Flag;
    switch (Flag) {
      case 0: // Desligar
        relayOff.pressButton();
        break;;

      case 1: // Ligar
        relayOn.pressButton();
        break;;

      default:
        debug_print(PSTR("Invalid Button"));
        break;;
    }
  }

  unsigned int needdrift = t % (POOLINGTIME/1000UL);
  if (needdrift)
  {
    int period = (POOLINGTIME/1000UL) - needdrift;

    TimerFixer *t = new TimerFixer (period * 1000UL, me);
    t->startDelayed();
    
    debug_print(PSTR("tasklet Need to drift %d seconds"), period);
  }

  return;
}
