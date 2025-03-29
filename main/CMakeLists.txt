file(GLOB_RECURSE srcs 
    "main.c"
    "src/*.c"
    "audio_driver.c"
    "http_server.c"
)

# 添加组件依赖
set(COMPONENT_REQUIRES 
    esp_http_server
    esp_codec_dev
    led_strip
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "./include"
                       REQUIRES "${COMPONENT_REQUIRES}")

# 添加web内容文件
list(APPEND WEB_CONTENT 
    "web_content/eq_setting.html"
)
