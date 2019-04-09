#ifndef AUX_FUNCTIONS_H
#define AUX_FUNCTIONS_H

void format_read_data(void);
int check_I2C(void);
int getDeviceId(void);
void interpret_format_cmd(char*);
void interpret_rate_cmd(char* msg);
void calculate_mg_per_lsb(void);

#endif /*AUX_FUNCTIONS_H*/
