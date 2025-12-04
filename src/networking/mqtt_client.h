#ifndef MQTT_CLIENT_CONTROLLER_H
#define MQTT_CLIENT_CONTROLLER_H

#include "model/system_state.h"

void mqtt_init(void);
void mqtt_publish_state(const sensors_state_t *st);

#endif // MQTT_CLIENT_CONTROLLER_H