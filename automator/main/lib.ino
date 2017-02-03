#undef __FILENAME__
#define __FILENAME__ "lib"
/* Template utilizado para montar mensagens de debug */
template<class T> int debug_print_hlp (uint8_t enabled, const __FlashStringHelper* file, const int line, const T fmt, ...)
{
  int r;
  va_list args;
  char *lpBuffer, buffer[MAXSTRINGBUFFER];

  if (enabled == MODEOFF)
    return 0;

  strcpy_P (buffer, (const char *) file);
  lpBuffer = buffer + strlen(buffer);
  r = snprintf_P (lpBuffer, sizeof(buffer) - strlen(buffer), PSTR(":%d "), line);

  va_start (args, fmt);
  lpBuffer += r;
  r += vsnprintf_P(lpBuffer, sizeof(buffer) - strlen(buffer), (const char *) fmt, args);
  va_end(args);

  Serial.println(buffer);

  return r;
}

/* Classe que empacota as operacoes do rele */
class Relay : public DelayRun {
  protected:
    
    struct {
      uint8_t  pin:5;
      uint8_t  tries:2;
      uint8_t  mode:1;
    } prop;

  private:
    boolean turnOn () {
      debug_print(PSTR("Turning on relay %d"), this->prop.pin);

      digitalWrite (this->prop.pin, LOW);
      return true;
    }

    boolean turnOff () {
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
      Relay *r = (static_cast<Relay*>(task));

      debug_print(PSTR("Property tries is %d, mode is %d"), r->prop.tries, r->prop.mode);

      switch (r->prop.mode) {
        case MODEON:
          r->turnOn();
          break;;
        
        case MODEOFF:
        default:
          r->turnOff();
          r->prop.tries--;
          break;;
      }

      r->prop.mode = (r->prop.mode + 1) & 0x1;
      
      if (r->prop.tries) {
        r->startDelayed();
      } else {
        delete r;
      }
    }
    
  public:
    Relay (unsigned long delayMs, int pin) : DelayRun (delayMs, Click) { this->prop.pin = pin; this->pressButton();}
    ~Relay()
    { debug_print(PSTR("Relay() going down for %04x"), this); }
};

/* Classe que ajusta o timer para terminar sempre em hora cheia */
class TimerFixer : public DelayRun {
  private:
    Task *target;

    /* Funcao auxiliar para corrigir a execucao do callback a cada hora cheia */
    static boolean timerFixer ( Task* task )
    {
      TimerFixer *t = (static_cast<TimerFixer*>(task));
      debug_print(PSTR("Running TimerFixer() for %04x"), t->target);
      t->target->lastCallTimeMicros = micros();
      
      delete t;
      return true;
    }

  public:
    TimerFixer (unsigned long delayMs, Task *target) : DelayRun (delayMs, timerFixer) { this->target = target; this->startDelayed(); }
    ~TimerFixer()
    { debug_print(PSTR("TimerFixer() going down for %04x"), this->target); }
    
};

/* Classe que ajusta o timer para terminar sempre em hora cheia */
class RTCSyncer : public DelayRun {
  private:
    Task *target;
    uint8_t tries;
    time_t times[TIMESAMPLES];

    /* Funcao auxiliar para corrigir a execucao do callback a cada hora cheia */
    static boolean sync ( Task* task )
    {
      RTCSyncer *syncer = (static_cast<RTCSyncer*>(task));
      debug_print(PSTR("Running RTCSyncer() for %04x"), syncer->target);

      syncer->tries++;
      syncer->times[syncer->tries] = getTimeFromRTC();

      if (syncer->tries >= TIMESAMPLES) {

        if (!syncer->isRTCSane())
          syncer->setRTCSane();
        
        delete syncer;
      }

      syncer->startDelayed();
      return true;
    }

    bool isRTCSane ()
    {
      int i;
      time_t rtc;
      time_t last = times[0];

      for (i = 1; i < TIMESAMPLES; i++)
      {
        if ( (times[i] <= last) || (times[i] >= ( 2 * (TIMESAMPLEMSEC/MSECS_PER_SEC))) )
          return false;

        last = times[i];
      }

      rtc = times[TIMESAMPLES-1];

      if ( abs(now() - rtc) > 1)
        setTime(rtc);

      doDrift(times[0]);

      return true;
    }

    void setRTCSane ()
    {
      time_t t = now();
      time_t source = getDateFromSource();

      if ( source > t)
        t = source;
      
      setDS3231time (second(t), minute(t), hour(t), weekday(t), day(t), month(t), year(t) - Y2KMARKFIX);
    }

    void doDrift(time_t t)
    {
      unsigned int drift = t % (TIMESYNCING/MSECS_PER_SEC);
      if (drift)
      {
        int period = (TIMESYNCING/MSECS_PER_SEC) - drift;
        new TimerFixer ((period * MSECS_PER_SEC) - MSECS_PER_SEC/4, target);
        debug_print(PSTR("timerlet Need to drift %d seconds"), period);
      }
    }

  public:
    RTCSyncer (unsigned long delayMs, time_t t, Task *target) : DelayRun (delayMs, sync) {
      this->tries = 1;
      this->times[0] = t;
      this->target = target;
      this->startDelayed();
    }

    ~RTCSyncer()
    { debug_print(PSTR("RTCSyncer() going down for %04x"), this->target); }
    
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
void setDS3231time(const byte second, const byte minute, const byte hour, const byte dayOfWeek, const byte dayOfMonth, const byte month, const byte year)
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
void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year)
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
  char buffer[MAXSTRINGBUFFER], *lpBuffer = buffer;
  int szwrote, buffersz = sizeof(buffer);
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

 #ifdef ENABLE_DEBUG
  // send it to the serial monitor
  szwrote = snprintf_P (lpBuffer, buffersz, PSTR("%02d:%02d:%02d %02d/%02d/%02d Day of week: "), hour, minute, second, dayOfMonth, month, year);
  lpBuffer += szwrote;
  buffersz -= szwrote;
  
  switch(dayOfWeek){
  case 1:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Sunday"));
    break;
  case 2:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Monday"));
    break;
  case 3:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Tuesday"));
    break;
  case 4:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Wednesday"));
    break;
  case 5:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Thursday"));
    break;
  case 6:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Friday"));
    break;
  case 7:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Saturday"));
    break;
  default:
    szwrote = snprintf_P (lpBuffer, buffersz, PSTR("Invalid"));
  }
  
  lpBuffer += szwrote;
  buffersz -= szwrote;
  
  debug_print(PSTR("%s"), buffer)
#endif // ENABLE_DEBUG

  TimeElements T = {(uint8_t)second, (uint8_t)minute, (uint8_t)hour, (uint8_t)0, (uint8_t)dayOfMonth, (uint8_t)month, (uint8_t)y2kYearToTm(year)};
  const time_t t = makeTime(T);
  
  return t;
}  

/* Funcao que empacota a verificacao de se o horario corrente eh horario de verao */
int ehHorarioDeVerao(const time_t now, const int Month, const int Year)
{
  if (cacheEhHorarioDeVerao.year != Year)
  {
    debug_print(PSTR("Fazendo cache do calculo do horario de verao"));
    cacheEhHorarioDeVerao.inicio = InicioHorarioVerao(Year);
    cacheEhHorarioDeVerao.fim = TerminoHorarioVerao(Year-1);
    cacheEhHorarioDeVerao.year = Year;
  }
  else
  { debug_print(PSTR("Usando horario de verao em cache")); }
  
  int horarioDeVerao = 0;
  if ( Month > MONTHSPERAYEAR/2 )
  {
    const time_t Inicio = cacheEhHorarioDeVerao.inicio;
    debug_print(PSTR("Inicio do horario de Verao: %lu"), Inicio);
    if ( now >= Inicio ) {
      horarioDeVerao = 1;
      debug_print(PSTR("Eh Horario de Verao"));
    }
    else
    { debug_print(PSTR("Eh Horario Normal")); }
  }
  else
  {
    const time_t Fim = cacheEhHorarioDeVerao.fim;
    debug_print(PSTR("Fim do horario de Verao: %lu"), Fim);
    if ( (now + SECS_PER_HOUR) < Fim ) {
      horarioDeVerao = 1;
      debug_print(PSTR("Eh Horario de Verao"));
    }
    else
    { debug_print(PSTR("Eh Horario Normal")); }
  }

  return horarioDeVerao;
}

/* Funcao de ajuda para imprimir o horario corrente */
void printcurrentdate(time_t t)
{
  if (ehHorarioDeVerao(t, month(t), year(t)))
  {
    t += SECS_PER_HOUR;
  }
    
  int ano = year(t), mes = month(t), dia = day(t), hora = hour(t), minuto = minute(t), segundo = second(t);
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

/* timestamp da data de compilacao */
time_t getDateFromSource()
{
  time_t t;
  char Month[4];
  char buffer[MAXSTRINGBUFFER];
  int m = 0, Day = 0, Year = 0, Hour = 0, Minute = 0, Second = 0;
  const char *today = PSTR(__TIMESTAMP__);

  strncpy_P(buffer, today, MAXSTRINGBUFFER);
  sscanf_P (buffer, PSTR("%*3s %3s %d %d %d:%d:%d"), Month, &Day, &Year, &Hour, &Minute, &Second);
  
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
