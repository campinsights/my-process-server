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
    // make the rest of the list null:
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
int num_waiting_msgs(struct Mailbox * mbox)
{
    // start at head of queue with zero messages:
    struct Message * current = mbox->msg_queue;
    int count = 0;
    // traverse the list one node at a time until we reach the end,
    // noting one more waiting message each time:
    while(current != NULL)
    {
        current = current->next;
        count++;
    }
    // 'count' should now be the correct number of messages, or
    // zero if the message queue is NULL:
    return count;
}