#undef __FILENAME__
#define __FILENAME__ "lib"
/* Funcao de ajuda para montar mensagens de debug */
int debug_print_hlp (uint8_t enabled, const __FlashStringHelper* file, const int line, const char *fmt, ...)
{
  va_list args;
  time_t tNow = now();
  char *pBuffer, buffer[MAXSTRINGBUFFER];

  if (enabled == MODEOFF)
    return 0;

  strcpy_P (buffer, (const char *) file);
  pBuffer = buffer + strlen(buffer);
  pBuffer += snprintf_P (pBuffer, sizeof(buffer) - strlen(buffer), PSTR(":%d %02d:%02d:%02d "), line, hour(tNow), minute(tNow), second(tNow));

  va_start (args, fmt);
  pBuffer += vsnprintf_P(pBuffer, sizeof(buffer) - strlen(buffer), fmt, args);
  va_end(args);

  Serial.println(buffer);

  return pBuffer - buffer;
}

/* Classe que ajusta o timer para terminar sempre em hora cheia */
class TimerFixer : public DelayRun
{
  private:
    Task *target;
    uint32_t drift;
    uint32_t lastMicros;

    /* Funcao auxiliar para corrigir a execucao do callback a cada hora cheia */
    static boolean timerFixer ( Task* task )
    {
      TimerFixer *tFixer = static_cast<TimerFixer*>(task);

      if ( tFixer->lastMicros == 0 )
      {
        debug_print(PSTR("TimerFixer() running for 0x%04x"), tFixer->target);
        tFixer->delayMs = tFixer->drift;
        tFixer->lastMicros = micros();
        tFixer->startDelayed();
        return true;
      }

      debug_print(PSTR("TimerFixer() fixing for 0x%04x"), tFixer->target);
      tFixer->target->lastCallTimeMicros = tFixer->lastMicros;
      delete tFixer;
      return true;
    }

  public:
    ~TimerFixer()
    { debug_print(PSTR("TimerFixer() going down for 0x%04x"), this); }
    TimerFixer (Task *target, uint32_t drift, uint32_t delayMs) : DelayRun (delayMs, timerFixer) {
      this->drift = drift;
      this->lastMicros = 0;
      this->target = target;
      this->startDelayed();
      debug_print(PSTR("New instance of TimerFixer() on 0x%04x"), this);
    }
};

/* Classe que empacota as operacoes do rele */
class Relay : public DelayRun
{
  protected:
    
    struct {
      uint8_t  pin:5;
      uint8_t  tries:2;
      uint8_t  mode:1;
    } prop;

  private:
    boolean turnOn() {
      debug_print(PSTR("Turning on relay %d"), this->prop.pin);

      digitalWrite (this->prop.pin, LOW);
      return true;
    }

    boolean turnOff() {
      debug_print(PSTR("Turning off relay %d"), this->prop.pin);

      digitalWrite (this->prop.pin, HIGH);
      return true;
    }

    void pressButton() {
      debug_print(PSTR("pressButton [%d]"), this->prop.pin);
      
      this->prop.mode = MODEOFF;
      this->prop.tries = PRESSBUTTONTRIES;

      this->turnOn();
      this->startDelayed();
    }

    static boolean Click ( Task* task ) {
      Relay *relay = static_cast<Relay*>(task);

      debug_print(PSTR("Property tries is %d, mode is %d"), relay->prop.tries, relay->prop.mode);

      switch (relay->prop.mode) {
        case MODEON:
          relay->turnOn();
          break;;
        
        case MODEOFF:
        default:
          relay->turnOff();
          relay->prop.tries--;
          break;;
      }

      relay->prop.mode = (relay->prop.mode + 1) & 0x1;
      
      if (relay->prop.tries) {
        relay->startDelayed();
      } else {
        delete relay;
      }
    }
    
  public:
    ~Relay()
    { debug_print(PSTR("Relay() going down for 0x%04x"), this); }
    Relay (int pin, unsigned long delayMs) : DelayRun (delayMs, Click) {
      this->prop.pin = pin;
      this->pressButton();
      debug_print(PSTR("New instance of Relay() on 0x%04x"), this);
    }
};

/* Classe que ajusta o timer para terminar sempre em hora cheia */
class RTCSyncer : public DelayRun
{
  private:
    Task *target;
    time_t times[TIMESAMPLES];
    uint8_t tries;

    /* Funcao auxiliar para corrigir a execucao do callback a cada hora cheia */
    static boolean rtcSyncer ( Task* task )
    {
      time_t tNow = now();
      time_t rtc = getTimeFromRTC();
      RTCSyncer *syncer = static_cast<RTCSyncer*>(task);

      if (syncer->tries < TIMESAMPLES)
      {
        debug_print(PSTR("Running RTCSyncer() for 0x%04x, property tries: %d"), syncer->target, syncer->tries);
        syncer->times[syncer->tries] = rtc;
        syncer->startDelayed();
        syncer->tries++;
        return true;
      }

      if (!(syncer->isRTCSane()))
      {
        delete syncer;
        return false;
      }

      int diff = tNow - rtc;
      if ( diff )
      {
        setTime(rtc);
        debug_print(PSTR("System time adjusted by %d seconds"), diff);
      }

      delete syncer;
      return true;
    }

    bool isRTCSane()
    {
      for (int i = 1; i < TIMESAMPLES; i++)
      {
        int diff = times[i] - times[i-1];
        if (diff != 1)
        {
          debug_print(PSTR("RTC not sane, time diff != 1, %d"), diff);
          return false;
        }
      }

      return true;
    }

  public:
    ~RTCSyncer()
    { debug_print(PSTR("RTCSyncer() going down for 0x%04x"), this); }
    RTCSyncer (Task *target, unsigned long delayMs) : DelayRun (delayMs, rtcSyncer)
    {
      this->tries = 0;
      this->target = target;
      this->startDelayed();
      debug_print(PSTR("New instance of RTCSyncer() on 0x%04x"), this);
    }
};

class SerialTask : public Task {
  public:
    ~SerialTask() {
      SoftTimer.remove(this);
      debug_print(PSTR("SerialTask() going down for 0x%04x"), this);
    }
    SerialTask (unsigned long periodMs, void (*callback)(Task*)) : Task (periodMs, callback) {
      debug_print(PSTR("New instance of SerialTask() on 0x%04x"), this);
    }
};

/* Retorna o domingo de páscoa de um determinado ano */
time_t DomingoDePascoa(const int ano)
{
  int a = ano % 19;
  int b = ano  / 100;
  int c = ano % 100;
  int d = b / 4;
  int e = b % 4;
  int f = (b + 8 ) / 25;
  int g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4;
  int k = c % 4;
  int L = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * L) / 451;
  int mes = (h + L - 7 * m + 114) / 31;
  int dia = ((h + L - 7 * m + 114) % 31) + 1;
  
  TimeElements T = {0, 0, 0, 0, (uint8_t) dia, (uint8_t)mes, (uint8_t)CalendarYrToTm(ano)};
  return makeTime(T);
}

/* Retorna o domingo de carnaval de um determinado ano */
time_t DomingoDeCarnaval(const int ano)
{
  time_t DomingoDeCarnaval = DomingoDePascoa(ano) - (49 * SECS_PER_DAY);
  return DomingoDeCarnaval;
}

/* Retorna a data de início do horário de verão de um determinado ano */
time_t InicioHorarioVerao(const int ano)
{
  // terceiro domingo de outubro
  TimeElements T = {0, 0, 0, 0, 1, 10, (uint8_t)CalendarYrToTm(ano)};
  time_t primeiroDeOutubro = makeTime(T);
  time_t primeiroDomingoDeOutubro = primeiroDeOutubro + ( ((8 - weekday(primeiroDeOutubro)) % 7) * SECS_PER_DAY );
  time_t terceiroDomingoDeOutubro = primeiroDomingoDeOutubro + (14 * SECS_PER_DAY);
  return terceiroDomingoDeOutubro;
}

/* Retorna a data de término do horário de verão de um determinado ano */
time_t TerminoHorarioVerao(const int ano)
{
  TimeElements T = {0, 0, 0, 0, 1, 2, (uint8_t)CalendarYrToTm(ano + 1)};
  time_t primeiroDeFevereiro = makeTime(T);
  time_t primeiroDomingoDeFevereiro = primeiroDeFevereiro + ( ((8 - weekday(primeiroDeFevereiro)) % 7) * SECS_PER_DAY );
  time_t terceiroDomingoDeFevereiro = primeiroDomingoDeFevereiro + (14 * SECS_PER_DAY);

  if (terceiroDomingoDeFevereiro != DomingoDeCarnaval(ano))
  { return terceiroDomingoDeFevereiro; }
  else
  { return terceiroDomingoDeFevereiro + (7 * SECS_PER_DAY); }
}

/* Convert normal decimal numbers to binary coded decimal */
byte decToBcd(const byte val)
{ return( (val/10*16) + (val%10) ); }

/* Convert binary coded decimal to normal decimal numbers */
byte bcdToDec(const byte val)
{ return( (val/16*10) + (val%16) ); }

/* Set RTC timer */
void setDS3231time(const uint8_t second, const uint8_t minute, const uint8_t hour, const uint8_t dayOfWeek, const uint8_t dayOfMonth, const uint8_t month, const uint8_t year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

/* Read RTC timer */
void readDS3231time(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *dayOfWeek, uint8_t *dayOfMonth, uint8_t *month, uint8_t *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  
  // request seven bytes of data from DS3231 starting from register 00h
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  
  *second = bcdToDec(Wire.read() & 0x7F);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3F);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

/* Funcao que empacota a tomada do horario do RTC e convertem em time_t */
const time_t getTimeFromRTC()
{
  char buffer[MAXSTRINGBUFFER], *pBuffer = buffer;
  uint16_t szwrote, buffersz = MAXSTRINGBUFFER;
  uint8_t second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  #ifdef ENABLE_DEBUG
  // send it to the serial monitor
  szwrote = snprintf_P (pBuffer, buffersz, PSTR("RTC: %02d:%02d:%02d %02d/%02d/%02d DoW: "), hour, minute, second, dayOfMonth, month, year);
  pBuffer += szwrote;
  buffersz -= szwrote;
  
  switch(dayOfWeek){
  case 1:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Sunday"));
    break;
  case 2:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Monday"));
    break;
  case 3:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Tuesday"));
    break;
  case 4:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Wednesday"));
    break;
  case 5:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Thursday"));
    break;
  case 6:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Friday"));
    break;
  case 7:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Saturday"));
    break;
  default:
    szwrote = snprintf_P (pBuffer, buffersz, PSTR("Invalid"));
  }
  
  pBuffer += szwrote;
  buffersz -= szwrote;
  
  debug_print(PSTR("%s"), buffer)
  #endif // ENABLE_DEBUG

  TimeElements T = {(uint8_t)second, (uint8_t)minute, (uint8_t)hour, (uint8_t)0, (uint8_t)dayOfMonth, (uint8_t)month, (uint8_t)y2kYearToTm(year)};
  return makeTime(T);
}  

/* Funcao que empacota a verificacao de se o horario corrente eh horario de verao */
int ehHorarioDeVerao(const time_t tNow, const uint16_t Year)
{
  if (cacheEhHorarioDeVerao.year != Year)
  {
    cacheEhHorarioDeVerao.year = Year;
    cacheEhHorarioDeVerao.fim = TerminoHorarioVerao(Year-1);
    cacheEhHorarioDeVerao.inicio = InicioHorarioVerao(Year) - SECS_PER_HOUR;

    debug_print(PSTR("Cache do horario de verao de %d"), Year);
    debug_print(PSTR("Inicio do horario de Verao: %lu"), cacheEhHorarioDeVerao.inicio);
    debug_print(PSTR("Fim do horario de Verao: %lu"), cacheEhHorarioDeVerao.fim + SECS_PER_HOUR);
  }
  
  if ( (tNow < cacheEhHorarioDeVerao.fim) || (tNow >= cacheEhHorarioDeVerao.inicio) )
    return 1;

  return 0;
}

/* Funcao de ajuda para imprimir o horario corrente */
void printcurrentdate(time_t tNow)
{
  if (ehHorarioDeVerao(tNow, year(tNow)))
    tNow += SECS_PER_HOUR;
    
  uint16_t ano = year(tNow);
  uint8_t mes = month(tNow), dia = day(tNow), hora = hour(tNow), minuto = minute(tNow), segundo = second(tNow);
  info_print(PSTR("*** now() is %02d/%02d/%04d %02d:%02d:%02d ***"), dia, mes, ano, hora, minuto, segundo);

  return;
}

/* Salva na EEPROM a quantidade de boots que o arduino fez */
uint16_t get_next_count(const uint8_t increment = 0)
{
  uint8_t i = 0;
  uint16_t count = 0;

  if (increment)
  {
    for (i = 0; i < EEPROMCELLSTOUSE; i++)
      if (EEPROM.read(i) != 0xff)
        break;

    if (i == EEPROMCELLSTOUSE)
    {
      debug_print (PSTR("Formatando EEPROM"));
      for (i = 0; i < EEPROMCELLSTOUSE; i++)
        EEPROM.write(i, 0);
    }

    count = EEPROM.read(0);
    for (i = 1; i < EEPROMCELLSTOUSE; i++)
      if (count != EEPROM.read(i))
        break;

    if (i == EEPROMCELLSTOUSE)
    {
      EEPROM.write(0, count+1);
    }
    else
    {
      EEPROM.write(i, count);
    }
  }

  count = 0;
  for (i = 0; i < EEPROMCELLSTOUSE; i++)
    count += EEPROM.read(i);

  return count;
}

/* pega o timestamp da data de compilacao */
time_t getDateFromSource()
{
  time_t t;
  char Month[4];
  char buffer[MAXSTRINGBUFFER];
  const char *today = PSTR(__TIMESTAMP__);
  uint16_t m = 0, Year = 0, Day = 0, Hour = 0, Minute = 0, Second = 0;

  strncpy_P(buffer, today, MAXSTRINGBUFFER);
  sscanf_P (buffer, PSTR("%*3s %3s %02d %02d:%02d:%02d %04d"), Month, &Day, &Hour, &Minute, &Second, &Year);
  
  switch (Month[0]) {
    case 'J': m = Month[1] == 'a' ? 1 : m = Month[2] == 'n' ? 6 : 7; break;
    case 'F': m = 2; break;
    case 'A': m = Month[2] == 'r' ? 4 : 8; break;
    case 'M': m = Month[2] == 'r' ? 3 : 5; break;
    case 'S': m = 9; break;
    case 'O': m = 10; break;
    case 'N': m = 11; break;
    case 'D': m = 12; break;
    default: debug_print(PSTR("Month was not decoded [%s]"), buffer); break;
  }

  TimeElements T = {(uint8_t)Second, (uint8_t)Minute, (uint8_t)Hour, (uint8_t)0, (uint8_t)Day, (uint8_t)m, (uint8_t)CalendarYrToTm(Year)};
  return makeTime(T);
}

/* Ajusta o proximo timer para uma hora cheia */
void doDrift(Task *target, time_t tNow, uint32_t rounding)
{
  uint16_t drift = tNow % (rounding/MSECS_PER_SEC);

  if (!drift)
    return;

  uint16_t period = (rounding/MSECS_PER_SEC) - drift;
  new TimerFixer (target, (drift * MSECS_PER_SEC), (period * MSECS_PER_SEC));
  debug_print(PSTR("doDrift %d/%ld seconds for 0x%04x"), period, rounding/MSECS_PER_SEC, target);
}

/* Atuar na serial por interrupcao ao invez de fazer pooling */
/* Se houver um SerialEvent, verifica por pedidos na serial */
void serialEvent() {
  SoftTimer.add(new SerialTask(0, serialtask));
}
