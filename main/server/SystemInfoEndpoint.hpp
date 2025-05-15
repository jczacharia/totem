// #pragma once
//
// #include "cJSON.h"
// #include "esp_err.h"
//
// namespace endpoints
// {
//     inline esp_err_t systemInfo(httpd_req_t* req)
//     {
//         httpd_resp_set_type(req, "application/json");
//         cJSON* root = cJSON_CreateObject();
//         esp_chip_info_t chip_info;
//         esp_chip_info(&chip_info);
//         cJSON_AddStringToObject(root, "version", IDF_VER);
//         cJSON_AddNumberToObject(root, "cores", chip_info.cores);
//         char* sys_info = cJSON_Print(root);
//         httpd_resp_sendstr(req, sys_info);
//         free(sys_info);
//         cJSON_Delete(root);
//         return ESP_OK;
//     }
// }
