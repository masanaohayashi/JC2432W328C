#include <Arduino.h>
#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <vector>
#include <cstring>

#include "LGFX_Driver.hpp"
#include "CST820.h"

using audio_tools::I2SStream;

//  A2DP (16bit->32bit)
static I2SStream i2s;
static BluetoothA2DPSink a2dp_sink;
static LGFX tft;
static SPIClass sdSPI(VSPI);
static CST820 touch(33, 32, 25, 21, I2C_ADDR_CST820);
static bool isA2dpConnected = false;
static constexpr const char* dev_name = "TWV2000C";
static std::vector<int32_t> sample_buffer;
static bool sdInitialized = false;
static char sdStatus[96] = "SD: Not initialized";
static char touchStatus[64] = "Touch: --";
static char lastTouchStatus[64] = "";
static int lineHeight = 0;
static int statusLineY = 0;
static int sdLineY = 0;
static int touchLineY = 0;

static void init_sd_card();
static void drawStatusLine(int y, const char* text, uint16_t fgColor);

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

static void drawStatusLine(int y, const char* text, uint16_t fgColor) {
    if (lineHeight <= 0) return;
    const uint16_t bgColor = lgfx::color565(0, 0, 0);
    tft.fillRect(0, y - lineHeight / 2, tft.width(), lineHeight, bgColor);
    tft.setTextColor(fgColor, bgColor);
    tft.setTextDatum(lgfx::textdatum_t::middle_center);
    tft.drawString(text, tft.width() / 2, y);
    tft.setTextColor(lgfx::color565(255, 255, 255));
    tft.setTextDatum(lgfx::textdatum_t::top_center);
}

static void init_sd_card() {
    constexpr int SD_CS = 5;
    constexpr int SD_SCK = 18;
    constexpr int SD_MISO = 19;
    constexpr int SD_MOSI = 23;
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, sdSPI, 10000000)) {
        snprintf(sdStatus, sizeof(sdStatus), "SD: Not found");
        Serial.println("[SD] Initialization failed");
        return;
    }

    sdInitialized = true;

    int fileCount = 0;
    char firstName[32] = "";
    File root = SD.open("/");
    if (root) {
        while (true) {
            File entry = root.openNextFile();
            if (!entry) break;
            ++fileCount;
            if (firstName[0] == '\0') {
                strncpy(firstName, entry.name(), sizeof(firstName) - 1);
                firstName[sizeof(firstName) - 1] = '\0';
            }
            entry.close();
        }
        root.close();
    }

    bool writeOk = false;
    bool readOk = false;
    String payload = String("Hello SD @") + String(millis());
    const char *testPath = "/a2dp_sd_test.txt";

    File wf = SD.open(testPath, FILE_WRITE);
    if (wf) {
        if (wf.println(payload)) {
            writeOk = true;
        }
        wf.close();
    }

    String readBack;
    File rf = SD.open(testPath, FILE_READ);
    if (rf) {
        readBack = rf.readStringUntil('\n');
        rf.close();
        readOk = (readBack.length() > 0);
    }

    Serial.printf("[SD] files=%d first='%s' write=%s read=%s data='%s'\n",
                  fileCount,
                  firstName[0] ? firstName : "",
                  writeOk ? "OK" : "NG",
                  readOk ? "OK" : "NG",
                  readBack.c_str());

    snprintf(sdStatus,
             sizeof(sdStatus),
             "SD: %s/%s files=%d",
             writeOk ? "W OK" : "W NG",
             readOk ? "R OK" : "R NG",
             fileCount);
}


void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("A2DP sink starting...");
    print_mem("boot");

    // LovyanGFX 初期化 (軽量なHello World表示)
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(lgfx::color565(0, 0, 0));
    tft.setTextColor(lgfx::color565(255, 255, 255));
    tft.setTextDatum(lgfx::textdatum_t::top_center);
    tft.setFont(&lgfx::fonts::AsciiFont8x16);
    tft.setTextSize(2);
    tft.drawString("TWV2000M", tft.width() / 2, 0);
    tft.setTextSize(1);
    tft.setTextDatum(lgfx::textdatum_t::middle_center);
    tft.drawString("Hello LovyanGFX", tft.width() / 2, tft.height() / 3);
    tft.setTextDatum(lgfx::textdatum_t::top_center);
    tft.drawString("Waiting for A2DP...", tft.width() / 2, tft.height() / 2 + 8);

    lineHeight = tft.fontHeight();
    statusLineY = tft.height() * 2 / 3;
    sdLineY = statusLineY + lineHeight + 4;
    touchLineY = sdLineY + lineHeight + 4;

    touch.begin();
    snprintf(touchStatus, sizeof(touchStatus), "Touch: --");
    drawStatusLine(statusLineY, "Waiting for A2DP...", lgfx::color565(0, 255, 128));

    init_sd_card();
    drawStatusLine(sdLineY, sdStatus, lgfx::color565(255, 255, 0));
    drawStatusLine(touchLineY, touchStatus, lgfx::color565(0, 192, 255));
    strncpy(lastTouchStatus, touchStatus, sizeof(lastTouchStatus) - 1);
    lastTouchStatus[sizeof(lastTouchStatus) - 1] = '\0';

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
    static uint32_t ts = 0;
    uint32_t now = millis();

    // --- Touch update ---
    uint16_t rawX = 0, rawY = 0;
    uint8_t gesture = 0;
    bool pressed = touch.getTouch(&rawX, &rawY, &gesture);
    if (pressed) {
        uint16_t dispX = rawY;
        uint16_t dispY = (tft.height() > 0) ? (tft.height() - 1 - rawX) : rawX;
        if (dispX >= tft.width())  dispX = tft.width() - 1;
        if (dispY >= tft.height()) dispY = tft.height() - 1;
        snprintf(touchStatus, sizeof(touchStatus), "Touch: %3u,%3u g=%02X", dispX, dispY, gesture);
    } else {
        snprintf(touchStatus, sizeof(touchStatus), "Touch: --");
    }

    if (strcmp(touchStatus, lastTouchStatus) != 0) {
        drawStatusLine(touchLineY, touchStatus, lgfx::color565(0, 192, 255));
        strncpy(lastTouchStatus, touchStatus, sizeof(lastTouchStatus) - 1);
        lastTouchStatus[sizeof(lastTouchStatus) - 1] = '\0';
    }

    // --- Periodic status update ---
    if (now - ts > 3000) {
        ts = now;
        print_mem("run");
        const char* status = isA2dpConnected ? "A2DP Connected" : "Waiting for A2DP...";
        drawStatusLine(statusLineY, status, lgfx::color565(0, 255, 128));
        drawStatusLine(sdLineY, sdStatus, lgfx::color565(255, 255, 0));
    }

    delay(10);
}
