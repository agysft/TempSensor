extern "C" {
  #include <stdlib.h>
  #include <string.h>
  #include <inttypes.h>
}
//#include <WConstants.h>
#include <Arduino.h>
#include <Wire.h>
#include "RTC8564.h"

#define RTC8564_SLAVE_ADRS	(0xA2 >> 1)
//#define BCD2Decimal(x)		(((x>>4)*10)+(x&0xf))

// Constructors ////////////////////////////////////////////////////////////////

RTC8564::RTC8564()
	: _seconds(0), _minutes(0), _hours(0), _days(0), _weekdays(0), _months(0), _years(0), _century(0)
{
}

void RTC8564::init(void)
{
	delay(100); //cut&try
	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x00);			// write reg addr 00
	Wire.write(0x20);			// 00 Control 1, STOP=1
	Wire.write(0x00);			// 01 Control 2
	Wire.write(0x00);			// 02 Seconds
	Wire.write(0x00);			// 03 Minutes
	Wire.write(0x09);			// 04 Hours
	Wire.write(0x01);			// 05 Days
	Wire.write(0x01);			// 06 Weekdays
	Wire.write(0x01);			// 07 Months
	Wire.write(0x01);			// 08 Years
	Wire.write(0x00);			// 09 Minutes Alarm
	Wire.write(0x00);			// 0A Hours Alarm
	Wire.write(0x00);			// 0B Days Alarm
	Wire.write(0x00);			// 0C Weekdays Alarm
	Wire.write(0x00);			// 0D CLKOUT
	Wire.write(0x00);			// 0E Timer control
	Wire.write(0x00);			// 0F Timer
	Wire.write(0x00);			// 00 Control 1, STOP=0
	Wire.endTransmission();
}

// Public Methods //////////////////////////////////////////////////////////////

// An interrupt occurs every ten minutes
void RTC8564::INT10Minutes(void)
{
	delay(10); //cut&try
    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00010001);     // TI/TP=1, AIE=0, TIE=1
    Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0F);			// write reg addr 0F "Timer"
    Wire.write(10);             // 10 minutes
    Wire.endTransmission();

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0E);			// write reg addr 0E "Timer Control"
    Wire.write(0b10000011);     // TE=1 & source clk is 1 minutes
    Wire.endTransmission();
}

// interrupt occurs every x0 minutes
void RTC8564::INT10Minutes2(void)
{
	uint8_t Minutes10;
    delay(10); //cut&try
    // clear INT
    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00000000);     // AIE=0 (INT = High), Clear AF bit
    Wire.endTransmission();
    delay(10); //cut&try
    // read minutes
    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x03);			// write reg addr 03 = Minutes
	Wire.endTransmission(false);
	Wire.requestFrom(RTC8564_SLAVE_ADRS, 1);
    Minutes10 = Wire.read();
    Minutes10 = (Minutes10 & 0x7f) >> 4;
    Minutes10++;
    if ( Minutes10 > 5){
        Minutes10 = 0;
    }
    Minutes10 = Minutes10 << 4;

    // Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	// Wire.write(0x0E);			// write reg addr 0E "Timer Control"
    // Wire.write(0b00000000);     // TE=0 Timer Disable
    // Wire.endTransmission();
    delay(10); //cut&try
	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x09);			// write reg addr 0F "Timer"
    Wire.write(Minutes10);      // set Minute Alarm ( now minutes + 10 minutes)
    Wire.write(0b10000000);     // mask Hour Alarm
    Wire.write(0b10000000);     // mask Day Alarm
    Wire.write(0b10000000);     // mask Weekday Alarm
    Wire.endTransmission();

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00000010);     // AIE=1(Ready for next INT)
    Wire.endTransmission();
}

// interrupt occurs every 00 second
void RTC8564::INT00Sec(void)
{
    delay(10); //cut&try
    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0E);			// write reg addr 0E "Timer Control"
    Wire.write(0b00000000);     // TE=0 Timer Disable
    Wire.endTransmission();

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00010001);     // TI/TP=1, AIE=0, TIE=1, Clear AF bit
    Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0e);			// write reg addr 0E
    Wire.write(0b10000011);     // TE=1,TD1/0=0b11:minutes
    Wire.write(0b00000001);     // every 1 minuts
    Wire.endTransmission();
}

// interrupt occurs every 8 hours
void RTC8564::INT8hours(void)
{
	uint8_t Hours8;
    delay(10); //cut&try
    // read minutes
    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x04);			// write reg addr 04 = Hours
	Wire.endTransmission(false);
	Wire.requestFrom(RTC8564_SLAVE_ADRS, 1);
    Hours8 = Wire.read();
    Hours8 = ((Hours8 & 0x3f) >> 4) * 10 + (Hours8 & 0x0f);   // BCD --> decimal
    Hours8 = Hours8 + 8;
    if ( Hours8 > 23){
        Hours8 = Hours8 - 24;
    }
    Hours8 = (((Hours8 - (Hours8 % 10))/10) << 4) | (Hours8 % 10);

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0E);			// write reg addr 0E "Timer Control"
    Wire.write(0b00000000);     // TE=0 Timer Disable
    Wire.endTransmission();

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00000010);     // TI/TP=0, AIE=1, TIE=0, Clear AF bit
    Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x09);			// write reg addr 0F "Timer"
    Wire.write(0b10000000);     // mask Minute Alarm
    Wire.write(Hours8);         // Hour Alarm ( now Hours + 8 hours)
    Wire.write(0b10000000);     // mask Day Alarm
    Wire.write(0b10000000);     // mask Weekday Alarm
    Wire.endTransmission();
}

// interrupt occurs on 4 o'clock
void RTC8564::INT4oclock(void)
{
    delay(10); //cut&try

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x0E);			// write reg addr 0E "Timer Control"
    Wire.write(0b00000000);     // TE=0 Timer Disable
    Wire.endTransmission();

    Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x01);			// write reg addr 01 "Control 2"
    Wire.write(0b00000010);     // TI/TP=0, AIE=1, TIE=0, Clear AF bit
    Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x09);			// write reg addr 0F "Timer"
    Wire.write(0b10000000);     // mask Minute Alarm
    Wire.write(0x04);           // Hour Alarm ( set 4 o'clock)
    Wire.write(0b10000000);     // mask Day Alarm
    Wire.write(0b10000000);     // mask Weekday Alarm
    Wire.endTransmission();
}

void RTC8564::begin(void)
{
	//Wire.begin(4,5);
	if(isvalid() == false)
		init();
}

void RTC8564::sync(uint8_t date_time[],uint8_t size)
{
	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x00);			// write reg addr 00
	Wire.write(0x20);			// 00 Control 1, STOP=1
	Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x02);			// write reg addr 02
	Wire.write(date_time, size);
	Wire.endTransmission();

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x00);			// write reg addr 00
	Wire.write(0x00);			// 00 Control 1, STOP=0
	Wire.endTransmission();
}

bool RTC8564::available(void)
{
	uint8_t buff[7];

	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x02);			// write reg addr 02
	Wire.endTransmission();

	Wire.requestFrom(RTC8564_SLAVE_ADRS, 7);

	for(int i=0; i<7; i++){
		if(Wire.available()){
			buff[i] = Wire.read();
		}
	}

	_seconds  = buff[0] & 0x7f;
	_minutes  = buff[1] & 0x7f;
	_hours	  = buff[2] & 0x3f;
	_days	  = buff[3] & 0x3f;
	_weekdays = buff[4] & 0x07;
	_months	  = buff[5] & 0x1f;
	_years	  = buff[6];
	_century  = (buff[5] & 0x80) ? 1 : 0;
	return (buff[0] & 0x80 ? false : true);
}

bool RTC8564::isvalid(void)
{
	Wire.beginTransmission(RTC8564_SLAVE_ADRS);
	Wire.write(0x02);			// write reg addr 02
	Wire.endTransmission();
	Wire.requestFrom(RTC8564_SLAVE_ADRS, 1);
	if(Wire.available()){
		uint8_t buff = Wire.read();
		return (buff & 0x80 ? false : true);
	}
	return false;
}

uint8_t RTC8564::seconds(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_seconds);
	return _seconds;
}

uint8_t RTC8564::minutes(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_minutes);
	return _minutes;
}

uint8_t RTC8564::hours(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_hours);
	return _hours;
}

uint8_t RTC8564::days(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_days);
	return _days;
}

uint8_t RTC8564::weekdays() const {
	return _weekdays;
}

uint8_t RTC8564::months(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_months);
	return _months;
}

uint8_t RTC8564::years(uint8_t format) const {
	//if(format == Decimal) return BCD2Decimal(_years);
	return _years;
}

bool RTC8564::century() const {
	return _century;
}


// Preinstantiate Objects //////////////////////////////////////////////////////

RTC8564 Rtc = RTC8564();
