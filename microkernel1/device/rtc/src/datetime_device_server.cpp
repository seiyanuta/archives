#include "rtc.h"
#include <resea.h>
#include <resea/cpp/io.h>
#include <resea/io.h>
#include <resea/rtc.h>
#include <resea/datetime_device.h>

#define RTC_REG_YEAR     0x09
#define RTC_REG_CENTURY  0x32
#define RTC_REG_MONTH    0x08
#define RTC_REG_DAY      0x07
#define RTC_REG_HOUR     0x04
#define RTC_REG_MIN      0x02
#define RTC_REG_SEC      0x00
#define RTC_REG_STATUS_B 0x0b


static uint8_t read_cmos(int reg) {

    io_write8(resea::interfaces::io::SPACE_PORT, 0, 0x70, reg);
    return io_read8(resea::interfaces::io::SPACE_PORT, 0, 0x71);
}


static uint32_t bcd_to_binary(uint32_t x) {
    uint32_t r = 0;

    for (int i=1; x > 0; i*=10) {
        r += (x & 0x0f) * i;
        x >>= 4;
    }

    return r;
}


namespace rtc {
namespace datetime_device_server {

/** handles datetime_device.get_date */
void handle_get_date(channel_t __ch) {
    uint8_t year90s, century, month, day, hour, min, sec;
    uint32_t year, date;

    year90s  = read_cmos(RTC_REG_YEAR);
    century  = read_cmos(RTC_REG_CENTURY);
    month    = read_cmos(RTC_REG_MONTH);
    day      = read_cmos(RTC_REG_DAY);
    hour     = read_cmos(RTC_REG_HOUR);
    min      = read_cmos(RTC_REG_MIN);
    sec      = read_cmos(RTC_REG_SEC);

    if (!(read_cmos(RTC_REG_STATUS_B) & 0x04)) {
        // bytes are stored in BCD format, convert them
        year90s = bcd_to_binary(year90s);
        century = bcd_to_binary(century);
        month   = bcd_to_binary(month);
        day     = bcd_to_binary(day);
        hour    = bcd_to_binary(hour);
        min     = bcd_to_binary(min);
        sec     = bcd_to_binary(sec);
    }

    year = (century * 100) + year90s;
    date = (month << 22) | (day << 17) | (hour << 12) | (min << 6) | sec;

    INFO("date: %d-%d-%dT%d:%d:%d", year, month, day, hour, min, sec);
    resea::interfaces::datetime_device::send_get_date_reply(__ch, OK, year, date, 0);
}

} // namespace datetime_device_server
} // namespace rtc
