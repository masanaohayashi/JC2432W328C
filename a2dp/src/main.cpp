#include <Arduino.h>
#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include <esp_heap_caps.h>
#include <vector>

using audio_tools::I2SStream;

//  A2DP (16bit->32bit)
static I2SStream i2s;
static BluetoothA2DPSink a2dp_sink;
static bool isA2dpConnected = false;
static constexpr const char* dev_name = "TWV2000C";
static std::vector<int32_t> sample_buffer;

// callback 
void get_audio_data(const uint8_t *data, uint32_t len) {
    if (data == nullptr || len == 0 || !i2s) {
        return;
    }

    const size_t sample_count = len / sizeof(int16_t);
    if (sample_buffer.size() < sample_count) {
        sample_buffer.resize(sample_count);
    }

    const int16_t* data16 = reinterpret_cast<const int16_t*>(data);
    for (size_t idx = 0; idx < sample_count; ++idx) {
         const int16_t sample_with_error = data16[idx] | 0x0001;  // PCM5102A対策
         sample_buffer[idx] = static_cast<int32_t>(sample_with_error) << 16;
    }

    i2s.write(reinterpret_cast<const uint8_t*>(sample_buffer.data()),
              sample_count * sizeof(int32_t));
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
    cfg.bits_per_sample = 32;
    cfg.pin_bck = 16;
    cfg.pin_ws = 17;
    cfg.pin_data = 4;

    if (!i2s.begin(cfg)) {
        Serial.println("Failed to initialize I2S");
        while (true) {
            delay(1000);
        }
    }

    sample_buffer.reserve(1024);

    // オーディオデータコールバックを設定し、内部I2S出力を無効にする
    a2dp_sink.set_stream_reader(get_audio_data, false);
    a2dp_sink.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void* ctx) {
        Serial.printf("A2DP connection state changed: %d\n", state);
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) isA2dpConnected = true;
        else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) isA2dpConnected = false;
    });
    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_volume(90); // 0..100
    a2dp_sink.start(dev_name);
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
