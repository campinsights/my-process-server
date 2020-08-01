/* allow strings to contain up to 120 characters, about one line of *
 * text on a wide terminal screen                                   */
#define STRING_SIZE 121  

/* define a "short string" as just long enoguh to encode a priority *
 * or type code                                                     */
#define SHORT_STRING 16

#ifndef IPCMSG_H_INCLUDED
#define IPCMSG_H_INCLUDED

/* ==== define message priorities --------------------------------- *
 * (note that not all values in the octal range have been used;     *
 * space is reserved for other priority levels should future        *
 * implementations require them)                                    *
 * ---------------------------------------------------------------- */

/* "SPAM" = informational or status message that can be safely      *
 * ignored or processed at leisure                                  */
#define PRIORITY_SPAM       0 

/* "BATCH" = message that can be handled at "batch process"         *
 * priority, for example when the process is idle                   */
#define PRIORITY_BATCH      1 

/* "NORMAL" = message that should be handled in the course of       *
 * normal process events                                            */
#define PRIORITY_NORMAL     4

/* "INTERRUPT" = indicates that other process work should be set    *
 * aside to handle this message                                     */
#define PRIORITY_INTERRUPT  7

/* "ALL" = get messages of all priority levels                      */
#define PRIORITY_ALL        -1

/* function to interpret priority code as a string                  */
void pri_str(char * priority_string, int priority_code);


/* ==== define message types -------------------------------------- *
 * the hierarchy of types is intended to mimic a 'typical'          *
 * interprocess workflow: the process sends a request, receives     *
 * status messages along the way, and then eventually receives the  *
 * requested result                                                 *
 * ---------------------------------------------------------------- */

/* "INFO" = provide information about the process that is sending   *
 * this message                                                     */
#define TYPE_INFO           0

/* "REQUEST" = request work or information from a partner process   */
#define TYPE_REQUEST        1

/* "STATUS" = status report on ongoing process work                 */
#define TYPE_STATUS         2

/* "RESULT" = provide the result requested in the original          *
 * TYPE_REQUEST message                                             */
#define TYPE_RESULT         3

/* octal values 4, 5, and 6 are reserved for future uses; octal     *
 * value 7 is reserved for system messages                          */
#define TYPE_SYSTEM         7

/* "ALL" = get messages of all types                                */
#define TYPE_ALL            -1

/* function to interpret type code as a string                      */
void typ_str(char * type_string, int type_code);


/* ==== define IPC MESSAGE LINES as a linked list ----------------- *
 * Each node is one line of text (up to 120 characters) plus a link *
 * to the next line in the message                                  */
struct Line
{
    char text[STRING_SIZE];
    struct Line *next;
};

/* ==== define IPC MESSAGE QUEUE as a linked list ----------------- *
 * Each node is a message. Each message has a sender identity, a    *
 * priority, a message type, a sub-list of message lines, and       *
 * pointers to the prev. and next messages in the list              */
struct Message
{
    char sender_mbox[STRING_SIZE];
    int priority;
    int type;
    int num_lines;
    struct Line *first_line; 
    struct Message *prev;
    struct Message *next;
};

/* ==== define IPC MAILBOX LIST as a linked list ------------------ *
 * The mailboxes are held in a hash table, but in case of hash      *
 * collisions, a linked list of mailboxes is kept at each hash      *
 * location. Each node is a mailbox with a message queue and links  *
 * to the prev. and next mailboxes in the list.                     */
struct Mailbox
{
    char mbox_name[STRING_SIZE];
    struct Message *first_msg;
    struct Mailbox *prev;
    struct Mailbox *next;
};

/* this function creates a new mailbox, adding it as a node after   *
 * the node specified by 'prev'                                     */
struct Mailbox * new_mbox(char * mbox_name, struct Mailbox * prev);

/* this function adds a new mailbox to the linked list whose head   *
 * is specified by 'head'; it returns the length of the list        */
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

/* this function determines how many messages of the given priority *
 * and type are waiting in a mailbox's message queue                */
int num_waiting_msgs(struct Mailbox * mbox, int priority, int type, char *sender);

/* this function retrieves the first waiting message of a given     *
 * priority and type then removes that message from the list        */
struct Message * fetch_first_message(struct Mailbox * mbox, int priority, int type, char *sender);

/* this function creates a new message with the specified priority  *
 * and type, adding it as a node after the node specified by 'prev' */
struct Message * new_message(int priority, int type, char *sender, struct Message * prev);

/* this function adds a new message with the specified priority and *
 * type to the message queue whose head is specified by 'head'; it  *
 * returns the new message                                          */
struct Message * add_message(struct Mailbox * mbox, int priority, int type, char *sender);

/* this function adds a line of text to an existing message; it     *
 * returns the number of lines in the message                       */
int add_line(struct Message * msg, char line[STRING_SIZE]);


#endif