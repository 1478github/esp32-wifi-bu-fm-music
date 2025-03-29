#include "driver/i2s.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_vol.h"
#include "tas5717.h"
#include "tas5717_reg.h"

// I2S引脚配置
#define I2S_SCLK_IO     10  // 串行时钟
#define I2S_WS_IO       11  // 字选择线
#define I2S_DOUT_IO     12  // 数据输出
#define I2S_MCLK_IO     13  // 主时钟(可选)

static audio_codec_data_if_t *i2s_data_if = NULL;
static audio_codec_if_t *codec = NULL;

// 在文件开头添加
static const char *TAG = "AUDIO_DRIVER";

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static void ble_init(void) {
    // 初始化蓝牙控制器
    esp_nimble_hci_init();
    
    // 初始化NimBLE协议栈
    nimble_port_init();
    
    // 初始化主机
    ble_hs_init();
    
    // 启动蓝牙任务
    nimble_port_freertos_init([](void *param) {
        nimble_port_run();
    });
}

// 在audio_driver_init()函数开头调用
void audio_driver_init(void) {
    ble_init();  // 添加蓝牙初始化
    // I2S配置
    // 修改I2S配置部分
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = NULL,
        .mclk_pin = I2S_MCLK_IO,
        .mclk_freq_hz = 12288000,
        .i2s_format = AUDIO_CODEC_I2S_FORMAT_I2S, // 明确指定I2S格式
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 明确指定采样位数
    };
    
    // 创建I2S数据接口
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    
    // 创建TAS5717编解码器实例
    tas5717_codec_cfg_t tas5717_cfg = {
        .ctrl_if = NULL,
        .data_if = i2s_data_if,
        .reset_pin = -1,
        .mclk_div = TAS5717_MCLK_DIV_256, // 明确MCLK分频系数
        .sample_rate = 48000, // 明确采样率
    };
    codec = audio_codec_new_tas5717(&tas5717_cfg);
    
    // 初始化TAS5717
    audio_codec_set_fs(codec, 48000, 16, 2); // 设置采样率
    audio_codec_ctrl(codec, AUDIO_CODEC_CTRL_POWER_UP, NULL); // 上电
    
    // 检查TAS5717模式
    check_tas5717_mode();
    
    // 增加MCLK频率设置
    set_mclk_frequency(codec, 48000);
    
    // 增加初始化状态检查
    if(codec == NULL || i2s_data_if == NULL) {
        ESP_LOGE(TAG, "音频驱动初始化失败");
        return;
    }
    ESP_LOGI(TAG, "音频驱动初始化完成");
}


void check_tas5717_mode(void) {
    uint8_t reg_value = 0;
    
    // 增加错误检查
    esp_err_t ret = audio_codec_reg_read(codec, TAS5717_SYS_CTRL_1, &reg_value);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "读取TAS5717寄存器失败: %d", ret);
        return;
    }
    
    // 更全面的模式检查
    if((reg_value & 0x0B) == 0x08) {  // 检查bit3和bit1
        ESP_LOGI(TAG, "TAS5717已正确配置为I2S模式");
    } else {
        ESP_LOGE(TAG, "TAS5717模式配置错误，寄存器值: 0x%02X", reg_value);
        set_tas5717_i2s_mode();  // 自动尝试修复
    }
}


void set_tas5717_i2s_mode(void) {
    // 设置系统控制寄存器1为I2S模式
    audio_codec_reg_write(codec, TAS5717_SYS_CTRL_1, 0x05);
    // 设置系统控制寄存器2为I2S格式
    audio_codec_reg_write(codec, TAS5717_SYS_CTRL_2, 0x00);
}


void set_mclk_frequency(audio_codec_if_t *codec, int sample_rate) {
    if(codec == NULL) {
        ESP_LOGE(TAG, "无效的编解码器句柄");
        return;
    }
    
    int mclk_freq = sample_rate * 256;
    esp_err_t ret = audio_codec_set_fs(codec, sample_rate, 16, 2);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "设置采样率失败: %d", ret);
        return;
    }
    
    audio_codec_i2s_cfg_t *i2s_cfg = (audio_codec_i2s_cfg_t *)codec->get_interface(codec, AUDIO_CODEC_DATA_INTERFACE);
    if(i2s_cfg == NULL) {
        ESP_LOGE(TAG, "获取I2S配置失败");
        return;
    }
    
    i2s_cfg->mclk_freq_hz = mclk_freq;
    ESP_LOGI(TAG, "成功设置MCLK频率为: %dHz", mclk_freq);
}