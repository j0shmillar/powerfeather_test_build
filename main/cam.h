#ifndef CAM_H
#define CAM_H

#include <esp_err.h>
#include <esp_camera.h>

#define CAM_PIN_PWDN  -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK   2   //XCLK
#define CAM_PIN_SIOD  35   //SDA
#define CAM_PIN_SIOC  36   //SCL

#define CAM_PIN_D7    45
#define CAM_PIN_D6    12
#define CAM_PIN_D5    17
#define CAM_PIN_D4    18
#define CAM_PIN_D3    15
#define CAM_PIN_D2    16
#define CAM_PIN_D1    37
#define CAM_PIN_D0     6
#define CAM_PIN_VSYNC  3   //VSYNC
#define CAM_PIN_HREF   8   //HS
#define CAM_PIN_PCLK   1   //PC

esp_err_t init_cam(void);
void capture(void);

#endif