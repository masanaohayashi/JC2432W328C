#include "CST820.h"

CST820::CST820(int8_t sda_pin, int8_t scl_pin, int8_t rst_pin, int8_t int_pin, uint8_t addr)
{
    _sda = sda_pin;
    _scl = scl_pin;
    _rst = rst_pin;
    _int = int_pin;
    _addr = addr;
}

void CST820::begin(void)
{
    if (_sda != -1 && _scl != -1) {
        Wire.begin(_sda, _scl);
    } else {
        Wire.begin();
    }
    // 元の動作に戻す（400kHz）
    Wire.setClock(400000);

    if (_int != -1) {
        pinMode(_int, OUTPUT);
        digitalWrite(_int, HIGH);
        delay(1);
        digitalWrite(_int, LOW);
        delay(1);
        // 以前は入力に戻していなかったので元に戻す
    }

    if (_rst != -1) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(300);
    }

    i2c_write(0xFE, 0xFF);
}

bool CST820::getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture)
{
    bool FingerIndex = false;
    FingerIndex = (bool)i2c_read(0x02);

    *gesture = i2c_read(0x01);
    if (!(*gesture == SlideUp || *gesture == SlideDown)) {
        *gesture = None;
    }

    uint8_t data[4];
    i2c_read_continuous(0x03, data, 4);
    *x = ((data[0] & 0x0f) << 8) | data[1];
    *y = ((data[2] & 0x0f) << 8) | data[3];

    return FingerIndex;
}

uint8_t CST820::i2c_read(uint8_t addr)
{
    uint8_t rdData = 0;
    uint8_t rdDataCount;
    do {
        Wire.beginTransmission(_addr);
        Wire.write(addr);
        Wire.endTransmission(false);
        rdDataCount = Wire.requestFrom(_addr, (uint8_t)1);
    } while (rdDataCount == 0);
    while (Wire.available()) {
        rdData = Wire.read();
    }
    return rdData;
}

uint8_t CST820::i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length)
{
  Wire.beginTransmission(_addr);
  Wire.write(addr);
  if (Wire.endTransmission(true)) return (uint8_t)-1;
  Wire.requestFrom(_addr, (uint8_t)length);
  for (uint32_t i = 0; i < length; i++) {
    if (Wire.available()) *data++ = Wire.read();
  }
  return 0;
}

void CST820::i2c_write(uint8_t addr, uint8_t data)
{
    Wire.beginTransmission(_addr);
    Wire.write(addr);
    Wire.write(data);
    Wire.endTransmission();
}

uint8_t CST820::i2c_write_continuous(uint8_t addr, const uint8_t *data, uint32_t length)
{
  Wire.beginTransmission(_addr);
  Wire.write(addr);
  for (uint32_t i = 0; i < length; i++) {
    Wire.write(*data++);
  }
  if (Wire.endTransmission(true)) return (uint8_t)-1;
  return 0;
}
