/* Classe que empacota as operacoes do rele */
class Relay : public DelayRun {
  protected:
    uint8_t pin;
    struct {
      uint8_t  mode:1;
      uint8_t tries:7;
    } prop;

  private:
    boolean turnOn () {
      debug_print(PSTR("Turning on relay %d"), this->pin);

      digitalWrite (this->pin, LOW);
      return true;
    }

    boolean turnOff () {
      debug_print(PSTR("Turning off relay %d"), this->pin);

      digitalWrite (this->pin, HIGH);
      return true;
    }

    static boolean Click ( Task* task ) {
      Relay *r = (static_cast<Relay*>(task));

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

      if (r->prop.tries)
        r->startDelayed();
    }

  public:
    Relay (unsigned long delayMs, int pin) : DelayRun (delayMs, Click) { this->pin = pin; }
    
    void pressButton() {
      debug_print(PSTR("pressButton [%d]"), this->pin);
      
      this->prop.mode = MODEOFF;
      this->prop.tries = PRESSBUTTONTRIES;

      this->turnOn();
      this->startDelayed();
    }
};

/* Classe que ajusta o timer para terminar sempre em hora cheia */
class TimerFixer : public DelayRun {
  protected:
    Task *target;

  public:
    TimerFixer (unsigned long delayMs, Task *target) : DelayRun (delayMs, timerFixer) { this->target = target; }
    ~TimerFixer()
    {
      debug_print(PSTR("TimerFixer() going down for %04x"), this->target);
    }
    
    /* Funcao auxiliar para corrigir a execucao do callback a cada hora cheia */
    static boolean timerFixer ( Task* task )
    {
      TimerFixer *t = (static_cast<TimerFixer*>(task));
      debug_print(PSTR("Running TimerFixer() for %04x"), t->target);
      t->target->lastCallTimeMicros = micros();
      
      delete t;
      return true;
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
    if ( (now + SECONDS_IN_HOUR) < Fim ) {
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
  char buffer[MAXSTRINGBUFFER];
  
  if (ehHorarioDeVerao(t, month(t), year(t)))
  {
    t += SECS_PER_HOUR;
  }
    
  int ano = year(t), mes = month(t), dia = day(t), hora = hour(t), minuto = minute(t), segundo = second(t);
  
  snprintf_P(buffer, sizeof(buffer),PSTR("*** now() is %02d/%02d/%04d %02d:%02d:%02d ***"), dia, mes, ano, hora, minuto, segundo);
  Serial.println(buffer);

  return;
}

/* Se o RTC e a libc nao retornarem um valor valido, usar a data de compilacao como sendo a data corrente */
void setDateFromSource(time_t *t)
{
  char Month[4];
  char buffer[MAXSTRINGBUFFER];
  int m, Day, Year, Hour, Minute, Second;
  const char *today = PSTR(__DATE__ " " __TIME__ );

  strncpy_P(buffer, today, MAXSTRINGBUFFER);
  sscanf_P (buffer, PSTR("%3s %d %d %d:%d:%d"), Month, &Day, &Year, &Hour, &Minute, &Second);
  switch (Month[0]) {
    case 'J': m = Month[1] == 'a' ? 1 : m = Month[2] == 'n' ? 6 : 7; break;
    case 'F': m = 2; break;
    case 'A': m = Month[2] == 'r' ? 4 : 8; break;
    case 'M': m = Month[2] == 'r' ? 3 : 5; break;
    case 'S': m = 9; break;
    case 'O': m = 10; break;
    case 'N': m = 11; break;
    case 'D': m = 12; break;
  }

  TimeElements T = {(uint8_t)Second, (uint8_t)Minute, (uint8_t)Hour, (uint8_t)0, (uint8_t)Day, (uint8_t)m, (uint8_t)CalendarYrToTm(Year)};
  *t = makeTime(T);
  
  int EhHorarioDeVerao = ehHorarioDeVerao(*t, month(*t), year(*t));
  if (EhHorarioDeVerao)
  {
    
    *t -= SECS_PER_HOUR;
    debug_print(PSTR("ehHorarioDeVerao returned true"));
  }

  setTime(*t);
  
  debug_print(PSTR("Just booted, using converted hardcoded time from '%s' to %ld"), buffer, *t);
  return;
}
