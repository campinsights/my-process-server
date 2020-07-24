#include <unistd.h>

void write_string(int fd, char *str)
{
    int i = 0;
    char out_char = str[i];
    // write characters one at a time until null terminator 
    // is found
    while(out_char != '\0')
    {
        write(fd, &out_char, sizeof(char));
        out_char = str[++i];
    }
    // be sure to write the null terminator
    write(fd, &out_char, sizeof(char));
}

void read_string(int fd, char *str, int max_size)
{
    int i = 0;
    char in_char = ' ';
    // read characters one at a time and append to str
    // until a null terminator is found or max_size is reached
    while(in_char != '\0' && i < max_size)
    {
        read(fd, &in_char, sizeof(char));
        str[i++] = in_char;
    }
    // continue reading until null is found;
    // this empties the buffer and makes sure the string
    // is null-terminated
    while(in_char != '\0')
    {
        read(fd, &in_char, sizeof(char));
        str[max_size - 1] = in_char;
    }
}

void write_int(int fd, int *int_to_write)
{
    write(fd, int_to_write, sizeof(int));
}

void read_int(int fd, int *int_to_read)
{
    read(fd, int_to_read, sizeof(int));
}
