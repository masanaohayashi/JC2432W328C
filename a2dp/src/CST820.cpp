#include "CST820.h"

CST820::CST820(int8_t sda_pin, int8_t scl_pin, int8_t rst_pin, int8_t int_pin, uint8_t addr)
    : _sda(sda_pin), _scl(scl_pin), _rst(rst_pin), _int(int_pin), _addr(addr) {}

void CST820::begin() {
    if (_sda != -1 && _scl != -1) {
        Wire.begin(_sda, _scl);
    } else {
        Wire.begin();
    }
    Wire.setClock(400000);

    if (_int != -1) {
        pinMode(_int, OUTPUT);
        digitalWrite(_int, HIGH);
        delay(1);
        digitalWrite(_int, LOW);
        delay(1);
    }
    if (_rst != -1) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(300);
    }
    i2c_write(0xFE, 0xFF);  // disable auto low power
}

bool CST820::getTouch(uint16_t* x, uint16_t* y, uint8_t* gesture) {
    bool finger = static_cast<bool>(i2c_read(0x02));
    if (gesture != nullptr) {
        *gesture = i2c_read(0x01);
    } else {
        (void)i2c_read(0x01);
    }
    uint8_t data[4] = {0};
    i2c_read_cont(0x03, data, sizeof(data));
    if (x) *x = ((data[0] & 0x0F) << 8) | data[1];
    if (y) *y = ((data[2] & 0x0F) << 8) | data[3];
    return finger;
}

uint8_t CST820::i2c_read(uint8_t reg) {
    uint8_t rd = 0;
    uint8_t cnt;
    do {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.endTransmission(false);
        cnt = Wire.requestFrom(_addr, static_cast<uint8_t>(1));
    } while (cnt == 0);

    while (Wire.available()) {
        rd = Wire.read();
    }
    return rd;
}

uint8_t CST820::i2c_read_cont(uint8_t reg, uint8_t* data, uint32_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission(true)) {
        return static_cast<uint8_t>(-1);
    }
    Wire.requestFrom(_addr, static_cast<uint8_t>(len));
    for (uint32_t i = 0; i < len; ++i) {
        if (Wire.available()) {
            data[i] = Wire.read();
        }
    }
    return 0;
}

void CST820::i2c_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

