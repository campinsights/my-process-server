/* allow strings to contain up to 255 characters */
#ifndef STRING_SIZE
#define STRING_SIZE 256  
#endif

#ifndef IPCMSG_H_INCLUDED
#define IPCMSG_H_INCLUDED

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

struct Message
{
    int priority;
    int type;
    char text[STRING_SIZE];
    struct Message *prev;
    struct Message *next;
};

struct Mailbox
{
    char mbox_name[STRING_SIZE];
    struct Message *msg_queue;
    struct Mailbox *prev;
    struct Mailbox *next;
};

/* this function creates a new mailbox, adding it as a node after   *
 * the node specified by 'prev'                                     */
struct Mailbox * new_mbox(char * mbox_name, struct Mailbox * prev);

/* this function adds a new mailbox to the linked list whose head   *
 * is specified by 'head' and returns the length of the list as an  *
 * integer return value                                             */
int add_mbox(struct Mailbox * head, char * mbox_name);

/* this function returns the list position of a named mailbox if it *
 * is in the list, or -1 if it is not                               */
int find_mbox(struct Mailbox * head, char * mbox_name);

/* this function returns the mailbox with the given mailbox name,   *
 * or NULL if the mailbox name does not exist                       */
struct Mailbox * get_mbox(struct Mailbox * head, char * mbox_name);

/* this function returns the mailbox at the given list position,    *
 * or NULL if the list does not contain that position index         */
struct Mailbox * get_mbox_at(struct Mailbox * head, int list_posn);

/* this function determines how many total messages are waiting in  *
 * a mailbox's message queue                                        */
int num_waiting_msgs(struct Mailbox * mbox);

#endif