#ifndef MSG_H_INCLUDED
#define MSG_H_INCLUDED

void write_string(int fd, char *str);

void read_string(int fd, char *str, int max_size);

void write_int(int fd, int *int_to_write);

void read_int(int fd, int *int_to_read);

#endif