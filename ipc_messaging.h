#ifndef LINKLIST_H_INCLUDED
#define LINKLIST_H_INCLUDED

/* allow strings to contain up to 255 characters */
#define STRING_SIZE 256  

/* define message priorities */
#define PRIORITY_HIGH   5
#define PRIORITY_MED    3
#define PRIORITY_LOW    1
#define PRIORITY_SPAM   0

/* define message types */
#define TYPE_INFO       0 // provide process information
#define TYPE_STATUS     1 // status update on process work
#define TYPE_REQUEST    2 // request for process work
#define TYPE_SYSTEM     7 // reserved for system use

typedef struct Message
{
    int priority;
    int type;
    char text[STRING_SIZE];
    Message *prev;
    Message *next;
};

typedef struct Mailbox
{
    char mbox_name[STRING_SIZE];
    Mailbox *prev;
    Mailbox *next;
};



#endif