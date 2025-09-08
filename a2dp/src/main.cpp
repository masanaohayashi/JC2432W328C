#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>

BluetoothA2DPSink a2dp;

static void print_mem(const char* stage) {
    size_t free8   = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t freeDMA = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    Serial.printf("[MEM %s] DRAM free=%u KB, largest=%u KB, DMA free=%u KB\n",
                  stage,
                  (unsigned)(free8/1024),
                  (unsigned)(largest/1024),
                  (unsigned)(freeDMA/1024));
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("A2DP sink starting...");
    print_mem("boot");

    // I2S pins (avoid conflicts with TFT/SD on CYD): LRCK=22, BCK=26, DATA=4
    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 22,
        .data_out_num = 4,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    a2dp.set_pin_config(pin_config);
    a2dp.set_auto_reconnect(true);
    a2dp.set_volume(90); // 0..100
    // Default I2S config (44.1kHz, 16bit, stereo) is fine for A2DP

    const char* dev_name = "CYD A2DP Sink"; // Bluetoothデバイス名
    a2dp.start(dev_name);

    Serial.printf("A2DP ready as '%s'\n", dev_name);
    Serial.println("- Pair and play from your phone/PC");
}

void loop() {
    static uint32_t ts = 0; uint32_t now = millis();
    if (now - ts > 3000) {
        ts = now;
        print_mem("run");
    }
    delay(10);
}

