// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Luigi Corsaro
// BSD/lwip socket headers for the raw TCP client to the Luxom DS65L.
// ESPHome on the arduino-esp32 core does NOT ship the Arduino WiFiClient, so we
// talk to the DS65L through the lwip POSIX socket API (always available on ESP-IDF).
// Referenced via `esphome: includes:`. Part of the firmware — you don't edit this.
#pragma once
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
