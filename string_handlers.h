#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

void write_string(int fd, char *str);

void read_string(int fd, char *str, int max_size);

#endif