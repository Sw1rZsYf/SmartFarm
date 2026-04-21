#ifndef _SNTP_H_
#define _SNTP_H_

#include "esp_sntp.h"
#include "time.h"

void initialize_sntp(void);
void calc_current_time(void);
void get_current_time_str(char *buf, size_t buf_len);

uint8_t get_current_hour(void);
uint8_t get_current_minute(void);

#endif // _SNTP_H_