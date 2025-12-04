FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS ${app_sources}

    INCLUDE_DIRS
        "."
        "controller"
        "drivers"
        "networking"
        "model"
        "view"
        REQUIRES    driver  freertos    esp_timer   u8g2    u8g2-hal-esp-idf
    
    EMBED_TXTFILES 
        "view/index.html" 
        "view/style.css" 
        "view/app.js"
)
