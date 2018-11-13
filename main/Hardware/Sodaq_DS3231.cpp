#include "Sodaq_DS3231.h"
#include "/users/rsn/esp/esp-idf/components/driver/include/driver/i2c.h"
#include "/users/rsn/esp/esp-idf/components/log/include/esp_log.h"

#define EPOCH_TIME_OFF              946684800  // This is 2000-jan-01 00:00:00 in epoch time
#define SECONDS_PER_DAY             86400L

#define DS3231_ADDRESS              0x68 //I2C Slave address

/* DS3231 Registers. Refer Sec 8.2 of application manual */
#define DS3231_SEC_REG              0x00
#define DS3231_MIN_REG              0x01
#define DS3231_HOUR_REG             0x02
#define DS3231_MDAY_REG             0x04
#define DS3231_MONTH_REG            0x05
#define DS3231_YEAR_REG             0x06

#define DS3231_AL1SEC_REG           0x07
#define DS3231_AL1MIN_REG           0x08
#define DS3231_AL1HOUR_REG          0x09
#define DS3231_AL1WDAY_REG          0x0A

#define DS3231_AL2MIN_REG           0x0B
#define DS3231_AL2HOUR_REG          0x0C
#define DS3231_AL2WDAY_REG          0x0D

#define DS3231_CONTROL_REG          0x0E
#define DS3231_STATUS_REG           0x0F
#define DS3231_AGING_OFFSET_REG     0x0F
#define DS3231_TMP_UP_REG           0x11
#define DS3231_TMP_LOW_REG          0x12

#define WRITE_BIT  					I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   					I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   				0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  				0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL   					0x0         /*!< I2C ack value */
#define NACK_VAL   					0x1         /*!< I2C nack value */

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

static const uint8_t daysInMonth [] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += daysInMonth [ i - 1];
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static uint32_t time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (long t) {
    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = daysInMonth [ m - 1];
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
    wday = 0;         // FIXME This is not properly initialized
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec, uint8_t wd) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = date;
    hh = hour;
    mm = min;
    ss = sec;
    wday = wd;
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using PSTR would further reduce the RAM footprint
DateTime::DateTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
    switch (date[0]) {
        case 'J': m = date[1] == 'a' ? 1 : date[2] == 'n' ? 6 : 7; break;
        case 'F': m = 2; break;
        case 'A': m = date[2] == 'r' ? 4 : 8; break;
        case 'M': m = date[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
    wday = 0;         // FIXME This is not properly initialized
}

uint32_t DateTime::get() const {
    uint16_t days = date2days(yOff, m, d);
    return time2long(days, hh, mm, ss);
}

uint32_t DateTime::getEpoch() const
{
    return get() + EPOCH_TIME_OFF;
}


static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

////////////////////////////////////////////////////////////////////////////////
// RTC DS3231 implementation


uint8_t Sodaq_DS3231::readRegister(uint8_t regaddress)
{
    uint8_t data_l;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regaddress, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL)
        return ret;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ESP_FAIL;
    }
    return data_l;
}

void Sodaq_DS3231::writeRegister(uint8_t regaddress,uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regaddress, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

uint8_t Sodaq_DS3231::begin(int i2cNum) {

  unsigned char ctReg=0;
 // ESP_LOGI("RSN" , "Channel %d",i2cNum);

    i2c_num = ( i2c_port_t)i2cNum;
    
  ctReg |= 0b00011100; 
  writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address
    vTaskDelay(10 / portTICK_RATE_MS);

  // set the clock to 24hr format
  uint8_t hrReg = readRegister(DS3231_HOUR_REG);
  hrReg &= 0b10111111;
  writeRegister(DS3231_HOUR_REG, hrReg);       

    vTaskDelay(10 / portTICK_RATE_MS);

  return 1;
}

//set the time-date specified in DateTime format
//writing any non-existent time-data may interfere with normal operation of the RTC
void Sodaq_DS3231::setDateTime(const DateTime& dt) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, DS3231_SEC_REG, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd(dt.second()), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd(dt.minute()), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd((dt.hour()) & 0b10111111), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, dt.dayOfWeek(), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd(dt.date()), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd(dt.month()), ACK_CHECK_EN);
    i2c_master_write_byte(cmd, bin2bcd(dt.year() - 2000), ACK_CHECK_EN);
    i2c_master_stop(cmd);
   i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

DateTime Sodaq_DS3231::makeDateTime(unsigned long t)
{
  if (t < EPOCH_TIME_OFF)
    return DateTime(0);
  return DateTime(t - EPOCH_TIME_OFF);
}

// Set the RTC using timestamp (seconds since epoch)
void Sodaq_DS3231::setEpoch(uint32_t ts)
{
  setDateTime(makeDateTime(ts));
}

//Read the current time-date and return it in DateTime format
DateTime Sodaq_DS3231::now() {

    uint8_t data[10];
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0, ACK_CHECK_EN);
    i2c_master_stop(cmd);
   i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS3231_ADDRESS << 1 |  READ_BIT, ACK_CHECK_EN);
    for(int a=0;a<7;a++)
        if(a<6)
        i2c_master_read_byte(cmd, &data[a], ACK_VAL);
    else
        i2c_master_read_byte(cmd, &data[a], NACK_VAL);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
  
  uint8_t ss = bcd2bin(data[0]);
  uint8_t mm = bcd2bin(data[1]);
   
  uint8_t hrreg = data[2];
  uint8_t hh = bcd2bin((hrreg & ~0b11000000)); //Ignore 24 Hour bit

  uint8_t wd =  data[3];
  uint8_t d = bcd2bin(data[4]);
  uint8_t m = bcd2bin(data[5]);
  uint16_t y = bcd2bin(data[6]) + 2000;
  
  return DateTime (y, m, d, hh, mm, ss, wd);
}

//Enable periodic interrupt at /INT pin. Supports only the level interrupt
//for consistency with other /INT interrupts. All interrupts works like single-shot counter
//Use refreshINTA() to re-enable interrupt.
void Sodaq_DS3231::enableInterrupts(uint8_t periodicity)
{

    unsigned char ctReg=0;
    ctReg |= 0b00011101; 
    writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address
    
   switch(periodicity) 
   {
       case EverySecond:
       writeRegister(DS3231_AL1SEC_REG,  0b10000000 ); //set AM1
       writeRegister(DS3231_AL1MIN_REG,  0b10000000 ); //set AM2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //set AM3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //set AM4

       break;

       case EveryMinute:
       writeRegister(DS3231_AL1SEC_REG,  0b00000000 ); //Clr AM1
       writeRegister(DS3231_AL1MIN_REG,  0b10000000 ); //set AM2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //set AM3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //set AM4

       break;

       case EveryHour:
       writeRegister(DS3231_AL1SEC_REG,  0b00000000 ); //Clr AM1
       writeRegister(DS3231_AL1MIN_REG,  0b00000000 ); //Clr AM2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //Set AM3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //set AM4

       break;
   }
}

//Enable HH/MM/SS interrupt on /INTA pin. All interrupts works like single-shot counter
void Sodaq_DS3231::enableInterrupts(uint8_t hh24, uint8_t mm, uint8_t ss)
{
    unsigned char ctReg=0;
    ctReg |= 0b00011101; 
    writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address

    writeRegister(DS3231_AL1SEC_REG,  0b00000000 | bin2bcd(ss) ); //Clr AM1
    writeRegister(DS3231_AL1MIN_REG,  0b00000000 | bin2bcd(mm)); //Clr AM2
    writeRegister(DS3231_AL1HOUR_REG, (0b00000000 | (bin2bcd(hh24) & 0b10111111))); //Clr AM3
    writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //set AM4
}

//Disable Interrupts. This is equivalent to begin() method.
void Sodaq_DS3231::disableInterrupts()
{
    begin(i2c_num); //Restore to initial value.
}

//Clears the interrrupt flag in status register. 
//This is equivalent to preparing the DS3231 /INT pin to high for MCU to get ready for recognizing the next INT0 interrupt
void Sodaq_DS3231::clearINTStatus()
{
    // Clear interrupt flag 
    uint8_t statusReg = readRegister(DS3231_STATUS_REG);
    statusReg &= 0b11111110;
    writeRegister(DS3231_STATUS_REG, statusReg);

}

//force temperature sampling and converting to registers. If this function is not used the temperature is sampled once 64 Sec.
void Sodaq_DS3231::convertTemperature()
{
    // Set CONV 
    uint8_t ctReg = readRegister(DS3231_CONTROL_REG);
    ctReg |= 0b00100000; 
    writeRegister(DS3231_CONTROL_REG,ctReg); 
 

    //wait until CONV is cleared. Indicates new temperature value is available in register.
    do
    {
       //do nothing
    } while ((readRegister(DS3231_CONTROL_REG) & 0b00100000) == 0b00100000 ); 
 
}

//Read the temperature value from the register and convert it into float (deg C)
float Sodaq_DS3231::getTemperature()
{
    float fTemperatureCelsius;
    uint8_t tUBYTE  = readRegister(DS3231_TMP_UP_REG);  //Two's complement form
    uint8_t tLRBYTE = readRegister(DS3231_TMP_LOW_REG); //Fractional part
  
    if(tUBYTE & 0b10000000) //check if -ve number
    {
       tUBYTE  ^= 0b11111111;  
       tUBYTE  += 0x1;
       fTemperatureCelsius = tUBYTE + ((tLRBYTE >> 6) * 0.25);
       fTemperatureCelsius = fTemperatureCelsius * -1;
    }
    else
    {
        fTemperatureCelsius = tUBYTE + ((tLRBYTE >> 6) * 0.25); 
    }
 
    return (fTemperatureCelsius);
      
}

Sodaq_DS3231 rtc;
