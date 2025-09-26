#include <Arduino.h>
#include <BluetoothA2DPSink.h>
//#include <AudioTools.h> // 追加
#include <driver/i2s.h>
#include <esp_heap_caps.h>



//  A2DP (16bit->32bit)
I2SStream i2s; // 変更
BluetoothA2DPSink a2dp_sink; // デフォルトコンストラクタを使用
bool isA2dpConnected = false;
const char* dev_name = "TWV2000C"; // Bluetoothデバイス名

// callback 
void get_audio_data(const uint8_t *data, uint32_t len) {
    // data is provided as array of int16 
    int sample_count = len / sizeof(int16_t);
    int16_t* data16 = reinterpret_cast<int16_t*>(const_cast<uint8_t*>(data)); // reinterpret_cast を使用
    for (int j=0; j<sample_count; j++){
         // add static error of 1 bit to each 16 bit sample (PCM5102A対策)
         int16_t sample_with_error = data16[j] | 0x0001;
         // convert to 32 bit data with sign extension
         int32_t sample32 = static_cast<int32_t>(sample_with_error) << 16; // Sign extension
        // output 32 bits to DAC
         i2s.write((uint8_t*)&sample32, sizeof(sample32)); // i2s.write を使用
    }
}

//  For memory check
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
    // 変更: AudioToolsを使ったピン設定
    auto cfg = i2s.defaultConfig();
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 32; // 32bitに変更
    cfg.pin_bck = 16;
    cfg.pin_ws = 17;
    cfg.pin_data = 4;
    i2s.begin(cfg);

    // オーディオデータコールバックを設定し、内部I2S出力を無効にする
    a2dp_sink.set_stream_reader(get_audio_data, false);
    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_volume(90); // 0..100
    a2dp_sink.start(dev_name);
    Serial.printf("A2DP ready as '%s'\n", dev_name);
    Serial.println("- Pair and play from your phone/PC");

    a2dp_sink.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void* ctx) {
        Serial.printf("A2DP connection state changed: %d\n", state);
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) isA2dpConnected = true;
        else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) isA2dpConnected = false;
    });
}

void loop() {
    static uint32_t ts = 0; uint32_t now = millis();
    if (now - ts > 3000) {
        ts = now;
        print_mem("run");
    }
    delay(10);
}
