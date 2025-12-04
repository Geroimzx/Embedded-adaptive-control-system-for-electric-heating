#pragma once
#include <stdbool.h>
#include <time.h>

bool weather_init(float latitude, float longitude, int update_interval_min); // запуск таску
void weather_deinit(void); // зупинити таск
void weather_set_location(float latitude, float longitude); // змінити координати
void weather_set_update_interval(int minutes); // змінити інтервал оновлення
float weather_get_temperature(void); // остання закешована температура, -1000 якщо немає
time_t weather_get_last_update_time(void); // час останнього оновлення