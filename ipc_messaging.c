#include "ipc_messaging.h"
#include <stdlib.h>
#include <string.h>

/* this function creates a new mailbox, adding it as a node after   *
 * the node specified by 'prev'                                     */
struct Mailbox * new_mbox(char * mbox_name, struct Mailbox * prev)
{
    // make space for a new mailbox:
    struct Mailbox * mbox = malloc(sizeof(struct Mailbox));
    // copy in the mailbox name:
    strcpy(mbox->mbox_name, mbox_name);
    // this is a new mailbox so its message queue should be empty:
    mbox->msg_queue = NULL;
    // make sure the prev pointer works:
    mbox->prev = prev;
    // we always add to the end of the list, so
    // next should be NULL:
    mbox->next = NULL;
    // return the new mailbox:
    return mbox;
}

/* this function adds a new mailbox to the linked list whose head   *
 * is specified by 'head' and returns the length of the list as an  *
 * integer return value                                             */
int add_mbox(struct Mailbox * head, char * mbox_name)
{
    // initialize list length to one (the head):
    int length = 1;
    // start at the head of the list and find the tail:
    struct Mailbox * tail = head;
    while(tail->next != NULL)
    {
        tail = tail->next;
        length++;
    }
    // now we are at the end of the list, so add the new mbox:
    tail->next = new_mbox(mbox_name, tail);
    length++;
    return length;
}

/* this function returns the list position of a named mailbox if it *
 * is in the list, or -1 if it is not                               */
int find_mbox(struct Mailbox * head, char * mbox_name)
{
    // start the position off at 0 and the current node at the head of the list:
    int position = 0;
    struct Mailbox * current = head;
    // move one position at at time until we run out of list or find the name:
    while(current != NULL && strcmp(current->mbox_name, mbox_name) != 0)
    {
        current = current->next;
        position++;
    }
    // report -1 if we reached the end of the list without finding the name,
    // else report the list position:
    if(current == NULL)
        return -1;
    else
        return position;
}

/* this function returns the mailbox with the given mailbox nare,   *
 * or NULL if the mailbox name does not exist                       */
struct Mailbox * get_mbox(struct Mailbox * head, char * mbox_name)
{
    // start off at the head of the list:
    struct Mailbox * current = head;
    // advance one node at a time until position is reached
    // or we run out of list nodes:
    while(current != NULL && strcmp(current->mbox_name, mbox_name) != 0)
    {
        current = current->next;
    }
    // return value will now be correct -- either the node with the given 
    // mailbox name or the value NULL if not found:
    return current;
}

/* this function returns the mailbox at the given list position,    *
 * or NULL if the list does not contain that position index         */
struct Mailbox * get_mbox_at(struct Mailbox * head, int list_posn)
{
    // start off at the head of the list and position 0:
    struct Mailbox * current = head;
    int position = 0;
    // advance one node at a time until position is reached
    // or we run out of list nodes:
    while(current != NULL && position < list_posn)
    {
        current = current->next;
        position++;
    }
    // return value will now be either the node at the given list
    // position or the value NULL if we ran out of nodes:
    return current;
}

/* this function determines how many total messages are waiting in  *
 * a mailbox's message queue                                        */
int num_waiting_msgs(struct Mailbox * mbox, int priority, int type)
{
    // start at head of queue with zero messages:
    struct Message * current = mbox->msg_queue;
    int count = 0;
    // traverse the list one node at a time until we reach the end,
    // noting one more waiting message each time:
    while(current != NULL)
    {
        if((priority == PRIORITY_ALL || current->priority == priority) && (type == TYPE_ALL || current->type == type))
            count++;
        current = current->next;
    }
    // 'count' should now be the correct number of messages, or
    // zero if the message queue is NULL:
    return count;
}

/* this function creates a new message with the specified priority  *
 * and type, adding it as a node after the node specified by 'prev' */
struct Message * new_message(int priority, int type, struct Message * prev)
{
    // make space for a new message:
    struct Message * msg = malloc(sizeof(struct Message));
    // set the priority and type:
    msg->priority = priority;
    msg->type = type;
    // this is a new message so its list of lines of text
    // should be empty:
    msg->firstLine = NULL;
    // make sure the prev pointer works:
    msg->prev = prev;
    // we always add to the end of the list, so
    // next should be NULL:
    msg->next = NULL;
    // return the new message:
    return msg;
}

/* this function adds a new message with the specified priority and *
 * type to the message queue whose head is specified by 'head'; it  *
 * returns the new message                                          */
struct Message * add_message(struct Mailbox * mbox, int priority, int type)
{
    int length;
    struct Message * tail = mbox->msg_queue;
    if (tail == NULL)
    {
        // if we reach this point we are starting a new list, so...
        tail = mbox->msg_queue = new_message(priority, type, NULL);
        length = 1;
    }
    else
    {
        // initialize length to 1 (the head)
        length = 1;
        // start at the head of the list and find the tail:
        while(tail->next != NULL)
        {
            tail = tail->next;
            length++;
        }
        // now we are at the end of the list, so add the new message:
        tail->next = new_message(priority, type, tail);
        tail = tail->next;
        length++;
    }
    return tail;
}

/* this function creates and returns a new line of message text     */
struct Line * new_line(char text[STRING_SIZE])
{
    // make space for a new line of text:
    struct Line * line = malloc(sizeof(struct Line));
    // copy over the message text:
    strcpy(line->text, text);
    // we always add to the end of the list, so
    // next should be NULL:
    line->next = NULL;
    // return the new message:
    return line;
}

/* this function adds a line of text to an existing message; it     *
 * returns the number of lines in the message                       */
int add_line(struct Message * msg, char line[STRING_SIZE])
{
    int num_lines;
    if(msg->firstLine == NULL)
    {
        msg->firstLine = new_line(line);
        num_lines = 1;
    }
    else
    {
        // initialize list length to one (the head of the list)
        num_lines = 1;
        struct Line * head = msg->firstLine;
        // start at the head of the list and find the tail:
        struct Line * tail = head;
        while(tail->next != NULL)
        {
            tail = tail->next;
            num_lines++;
        }
        // now we are at the end of the list, so add the new message:
        tail->next = new_line(line);
        num_lines++;
    }
    msg->lines = num_lines;
    return num_lines;
}


