#ifndef CST820_H
#define CST820_H

#include <Arduino.h>
#include <Wire.h>

#define I2C_ADDR_CST820 0x15

enum class CST820Gesture : uint8_t {
    None        = 0x00,
    SlideDown   = 0x01,
    SlideUp     = 0x02,
    SlideLeft   = 0x03,
    SlideRight  = 0x04,
    SingleTap   = 0x05,
    DoubleTap   = 0x0B,
    LongPress   = 0x0C
};

class CST820 {
public:
    CST820(int8_t sda_pin = -1, int8_t scl_pin = -1,
           int8_t rst_pin = -1, int8_t int_pin = -1,
           uint8_t addr = I2C_ADDR_CST820);

    void begin();
    bool getTouch(uint16_t* x, uint16_t* y, uint8_t* gesture);

private:
    int8_t _sda;
    int8_t _scl;
    int8_t _rst;
    int8_t _int;
    uint8_t _addr;

    uint8_t i2c_read(uint8_t reg);
    uint8_t i2c_read_cont(uint8_t reg, uint8_t* data, uint32_t len);
    void    i2c_write(uint8_t reg, uint8_t val);
};

#endif
