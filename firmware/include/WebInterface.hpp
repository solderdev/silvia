#pragma once

#include <Arduino.h>
#include "WebServer.h"  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/418
#include <ESPAsyncWebServer.h>
#include <esp_http_client.h>

class WebInterface
{
public:
  WebInterface();
  static void updateInfluxDB();
  SemaphoreHandle_t influx_sem_update;

private:
  static void task_http_wrapper(void *arg);
  void task_http();
  static void task_influx_wrapper(void *arg);
  void task_influx();
  static void task_ota_wrapper(void *arg);
  void task_ota();
  void wifiReconnect();
  void wifiCheckConnectionOrReconnect();

  AsyncWebServer server_;
  esp_http_client_config_t http_client_config_;
  esp_http_client_handle_t http_client_;
  
  TaskHandle_t task_handle_http_;
  TaskHandle_t task_handle_influx_;
  TaskHandle_t task_handle_ota_;

};
