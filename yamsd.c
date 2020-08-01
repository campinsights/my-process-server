#include "yams_headers.h"
#include "fio_handlers.h"
#include "ipc_messaging.h"
#include <time.h>

/* mark unused Client records as unused by setting their PID's and FD's *
 * to an illegal number (-1)                                            */
#define UNUSED -1

/*   -----~~~~~===== define necessary global variables =====~~~~~-----  */
int nextPID = 0;     // next available client process PID
int fd_syscall, fd_commchannel; // file pointers for incoming server FIFOs
int syscall_code;    // most recent incoming system call code
int connections = 0; // how many connected client processes
bool running = true; // whether the process server is supposed to
                     // still be running

/* create a structure to store client process information, sort of a
   process control block in miniature                                   */
struct Client {
    int PID;
    time_t start_time;
    char mailbox_name[STRING_SIZE];
    char fifo_name[STRING_SIZE];
    int fd_outgoing;
    int join_PID;
    int wait_PID;
    int recv_wait_priority;
    int recv_wait_type;
    char recv_wait_sender[STRING_SIZE];
} clients[LIST_SIZE];

/* create a hash table of mailboxes */
struct Mailbox *mboxes[LIST_SIZE];

/* the following function computes the hash code for a mailbox          */
int mbox_hash(char *mbox_name)
{
    // initialize sum to 0, then add each character value in turn
    int sum = 0;
    for(int i = 0; i < strlen(mbox_name); i++)
        sum += (int)mbox_name[i];
    // the hash code is the remainder when divided by the array size
    return sum % LIST_SIZE;
}

/* the following function registers a new mailbox and returns       *
 * a pointer to it                                                  */
struct Mailbox * register_mbox(char *mbox_name)
{
    int hash = mbox_hash(mbox_name);
    if(mboxes[hash]==NULL)
    {
        mboxes[hash] = new_mbox(mbox_name, NULL);
        printf("YAMSD: new mailbox %s created at hash %d\n", mbox_name, hash);
        return mboxes[hash];
    }
    else
    {
        int list_posn = find_mbox(mboxes[hash], mbox_name);
        if(list_posn < 0)
        {
            int list_len = add_mbox(mboxes[hash], mbox_name);
            printf("YAMSD: new mailbox %s added at hash %d to form a list of length %d\n", mbox_name, hash, list_len);
            // since list positions are zero-based, return mailbox at list_len - 1:
            return get_mbox_at(mboxes[hash], list_len - 1);
        }
        else
        {
            printf("YAMSD: mailbox %s already registered at hash %d, position %d\n", mbox_name, hash, list_posn);
            return get_mbox_at(mboxes[hash], list_posn);
        }
    }
}

/* the following function reads information from the server FIFO     *
 * to set up a new client struct and connect to a new client process */
void connect_process(struct Client *my_client)
{
    // CONNECT has two parameters -- int: linux PID, C-string: mailbox name
    int processLinuxPID;
    // read first parameter:
    read_int(fd_syscall, &processLinuxPID);
    // construct client FIFO name:
    sprintf(my_client->fifo_name, CLIENT_FIFO, processLinuxPID);
    printf("YAMSD: connecting Host-OS process #%d on named pipe %s\n", processLinuxPID, my_client->fifo_name);
    // read mailbox name:
    read_string(fd_commchannel, my_client->mailbox_name, STRING_SIZE);
    struct Mailbox * mbox = register_mbox(my_client->mailbox_name);
    int waiting = num_waiting_msgs(mbox, PRIORITY_ALL, TYPE_ALL, "*");
    printf("YAMSD: mailbox %s registered successfully; it has %d waiting messages\n", my_client->mailbox_name, waiting);
    // give client process a new PID:
    my_client->PID = nextPID;
    nextPID = (nextPID + 1) % LIST_SIZE;
    // assign client process a start time:
    time(&(my_client->start_time));
    // report client connection:
    printf("YAMSD: client process #%d has connected with mailbox %s at time %s\n", my_client->PID, my_client->mailbox_name, ctime(&(my_client->start_time)));
    // open client FIFO:
    my_client->fd_outgoing = open(my_client->fifo_name, O_WRONLY);
    // report successful client connection:
    printf("YAMSD: opened client FIFO at %s\n", my_client->fifo_name);
    // send PID back to client to confirm connection:
    printf("YAMSD: sending PID %d to client\n", my_client->PID);
    write_int(my_client->fd_outgoing, &(my_client->PID));
    // note that we are now connected to one additional client process:
    connections++;
    printf("YAMSD: connected to %d clients\n", connections);
}

/* handle connection failure gracefully */
void connect_fail()
{
    int clientPID;
    char param_string[STRING_SIZE];
    // we need to flush the info from the server FIFO so we can
    // service the next system call:
    read_int(fd_syscall, &clientPID);
    printf("YAMSD: rejecting connection from Linux process %d -- too many clients connected", clientPID);
    read_string(fd_syscall, param_string, STRING_SIZE);
    printf("YAMSD: rejecting request to connect mailbox %s", param_string);
    // TODO: add code to connect client FIFO long enough to send an error code
}

/* the following function disconnects from a client process     */
void disconnect_process(struct Client *my_client)
{
    // first, find out if any process has JOINed my_client
    // and send any that have a no-error (0) signal:
    for(int i = 0; i < LIST_SIZE; i++)
        if(clients[i].join_PID == my_client->PID)
        {
            clients[i].join_PID == UNUSED;
            int response_int = 0;
            write_int(clients[i].fd_outgoing, &response_int);
        }
    // now, disconnect my_client by closing FIFOs and 
    // marking this array slot and its file descriptor as UNUSED:
    printf("YAMSD: disconnecting from client %d.\n", my_client->PID);
    write_string(my_client->fd_outgoing, "DISCONNECTING. Goodbye.");
    close(my_client->fd_outgoing);
    my_client->PID = UNUSED;
    my_client->fd_outgoing = UNUSED;
    // note that we are now connected to one fewer client process:
    connections--;
    printf("YAMSD: connected to %d clients\n", connections);
}

void read_message(int clientPID, struct Message *msg)
{
    char message_line[STRING_SIZE];
    int lines = 0;
    do {
        read_string(fd_commchannel, message_line, STRING_SIZE);
        printf("YAMSD: received line %d = '%s'\n", ++lines, message_line);
        if(strlen(message_line) > 0)
            add_line(msg, message_line);
    } while(strlen(message_line) > 0);
    // we actually over-count lines by one because of the
    // last empty line, so...
    lines--;
    // client expects a confirmation, so...
    char response_string[STRING_SIZE * 2];
    sprintf(response_string, "Received %d message lines", lines);
    write_string(clients[clientPID].fd_outgoing, response_string);
    printf("YAMSD: finished receiving %d lines of text\n", lines);
}

void write_message(int clientPID, struct Message *msg)
{
    /* response takes the following form                                    *
     * - int: priority                                                      *
     * - int: message type                                                  *
     * - C-string: sender mailbox name                                      *
     * - int: number of lines                                               *
     * - (n) C-strings: the message                                         */
    write_int(clients[clientPID].fd_outgoing, &(msg->priority));
    write_int(clients[clientPID].fd_outgoing, &(msg->type));
    write_string(clients[clientPID].fd_outgoing, msg->sender_mbox);
    // let the client know how many lines we are about to send:
    int lines = msg->num_lines;
    write_int(clients[clientPID].fd_outgoing, &lines);
    printf("YAMSD: sending %d message lines to client %d\n", lines, clientPID);
    // now send lines one at a time:
    struct Line *this_line = msg->first_line;
    for (int i = 0; i < lines; i++)
    {
        // send the line of text and then advance the pointer
        write_string(clients[clientPID].fd_outgoing, this_line->text);
        this_line = this_line->next;
    }
    printf("YAMSD: message sent\n");

    // we are now done with this message, so we have to 
    // dispose of the memory that we used to hold it
    struct Line *next_line;
    this_line = msg->first_line;
    // first, free all of the memory allocated to the lines of text
    while (this_line != NULL)
    {
        next_line = this_line->next;
        free(this_line);
        this_line = next_line;
    }
    // now it is safe to free the message record itself
    free(msg);
}

/* the following function receives a client message             */
void receive_message(int clientPID)
{
    /* SEND takes these parameters:                                         *
     * - C-string: destination mailbox name                                 *
     * - int: priority                                                      *
     * - int: message type                                                  *
     * - (n) C-strings: the message                                         *
     * - one empty C-string as a message terminator                         */

    // read the mailbox name:
    char mbox_name[STRING_SIZE];
    read_string(fd_commchannel, mbox_name, STRING_SIZE);

    // read the message priority and type:
    int priority, type;
    char pri[SHORT_STRING], typ[SHORT_STRING];
    read_int(fd_commchannel, &priority);
    pri_str(pri, priority);
    read_int(fd_commchannel, &type);
    typ_str(typ, type);

    printf("YAMSD: receiving priority %s, type %s message from client %d for mailbox %s\n", pri, typ, clientPID, mbox_name);

    // before we store the message, find out if the client is waiting for it
    int i = 0;
    bool P = false, T = false, S = false;
    // step through the list until we find a match for this mailbox name
    // or run out of list entries
    while (i < LIST_SIZE && strcmp(clients[i].mailbox_name, mbox_name) != 0)
    {
        i++;
    }
    if (i != LIST_SIZE)
    {
        // if we get here, we found a matching mailbox
        P = clients[i].recv_wait_priority == PRIORITY_ALL || clients[i].recv_wait_priority == priority;
        T = clients[i].recv_wait_type == TYPE_ALL || clients[i].recv_wait_type == type;
        S = strcmp(clients[i].recv_wait_sender, "*") == 0 || strcmp(clients[i].recv_wait_sender, clients[clientPID].mailbox_name) == 0;
    }
    if (P && T && S)
    {
        // if we get here, we found a matching mailbox *and* 
        // the priority, type, and sender match the wait requirements
        struct Message * msg = new_message(priority, type, clients[clientPID].mailbox_name, NULL);

        // client expects a response at this point:
        char response_string[STRING_SIZE*2];
        sprintf(response_string, "Ready to receive priority %s, type %s message for mailbox %s", pri, typ, mbox_name);
        write_string(clients[clientPID].fd_outgoing, response_string);

        // now read the actual message:
        read_message(clientPID, msg);

        // and write it to the waiting client:
        write_message(i, msg);

        // and then mark the process as no longer waiting:
        clients[i].recv_wait_priority = UNUSED;
        clients[i].recv_wait_type = UNUSED;
        strcpy(clients[i].recv_wait_sender, "");
    }
    else
    {
        // if we get here, there is no waiting client
        // that matches P, T, and S criteria, so we file this message:

        // find the mailbox, creating it if it does not yet exist:
        struct Mailbox * mbox = register_mbox(mbox_name);
    
        // add a message to the list:
        struct Message * msg = add_message(mbox, priority, type, clients[clientPID].mailbox_name);

        // client expects a response at this point:
        char response_string[STRING_SIZE*2];
        sprintf(response_string, "Ready to receive priority %s, type %s message for mailbox %s", pri, typ, mbox_name);
        write_string(clients[clientPID].fd_outgoing, response_string);

        // now read the actual message:
        read_message(clientPID, msg);
    }
    
    
}

void check_messages(int clientPID)
{
    printf("YAMSD: received CHECK request for mailbox %s\n", clients[clientPID].mailbox_name);
    /* CHECK takes these parameters:                                        *
     * - int: priority to check for                                         *
     * - int: message type to check for                                     *
     * - C-string: sender mailbox name to check for                         */
    int priority, type;
    char pri[SHORT_STRING], typ[SHORT_STRING], sender[STRING_SIZE];
    read_int(fd_commchannel, &priority);
    pri_str(pri, priority);
    read_int(fd_commchannel, &type);
    typ_str(typ, type);
    read_string(fd_commchannel, sender, STRING_SIZE);
                    
    printf("YAMSD: checking for messages of priority %s and type %s from sender %s\n", pri, typ, sender);
    // first, get the mailbox (creating one if it does not exist)
    struct Mailbox * mbox = register_mbox(clients[clientPID].mailbox_name);
    int num_waiting = num_waiting_msgs(mbox, priority, type, sender);
    printf("YAMSD: found %d matching messages\n", num_waiting);
    char response_string[STRING_SIZE*2];
    sprintf(response_string, "You have %d messages of priority %s and type %s from sender %s", num_waiting, pri, typ, sender);
    write_string(clients[clientPID].fd_outgoing, response_string);
}

void fetch_message(int clientPID)
{
    /* RECV takes these parameters:                                         *
     * - int: priority                                                      *
     * - int: message type                                                  *
     * - C-string: sender mailbox name                                      */
    int priority, type;
    char pri[SHORT_STRING], typ[SHORT_STRING], sender[STRING_SIZE];
    read_int(fd_commchannel, &priority);
    pri_str(pri, priority);
    read_int(fd_commchannel, &type);
    typ_str(typ, type);
    read_string(fd_commchannel, sender, STRING_SIZE);

    printf("YAMSD: received FETCH(P: %s, T: %s, S: %s) request from client %d for mailbox %s\n", pri, typ, sender, clientPID, clients[clientPID].mailbox_name);
    // fetch the mailbox for the current client:
    struct Mailbox *mbox = register_mbox(clients[clientPID].mailbox_name);
    // fetch the first qualifying message:
    struct Message *msg = fetch_first_message(mbox, priority, type, sender);
    if (msg == NULL)
    {
        // no message found, so mark the process as waiting:
        printf("YAMSD: marking process %d as waiting for a message\n", clientPID);
        clients[clientPID].recv_wait_priority = priority;
        clients[clientPID].recv_wait_type = type;
        strcpy(clients[clientPID].recv_wait_sender, sender);
    }
    else
    {
        // message found, so send it: 
        write_message(clientPID, msg);       
    }
}

int main()
{
    // just in case we need it, get my host OS PID:
    int my_linux_PID = getpid();
    // this is almost as arbitrary a number as any, so 
    // use it to seed my random number generator:
    srand(my_linux_PID);

    // initialize all client records and mailboxes:
    for(int i = 0; i < LIST_SIZE; i++)
    {
        clients[i].PID = UNUSED;
        clients[i].join_PID = UNUSED;
        clients[i].wait_PID = UNUSED;
        clients[i].fd_outgoing = UNUSED;
        clients[i].recv_wait_priority = UNUSED;
        clients[i].recv_wait_type = UNUSED;
        strcpy(clients[i].recv_wait_sender, "");
        mboxes[i] = NULL;
    }

    while(running)
    {
        // "start up" the process server by creating 
        // named FIFO's for incoming connections:
        mkfifo(SERVER_FIFO_1, FIFO_MODE);
        printf("YAMSD: creating syscall FIFO at %s\n", SERVER_FIFO_1);
        mkfifo(SERVER_FIFO_2, FIFO_MODE);
        printf("YAMSD: creating comm-channel FIFO at %s\n", SERVER_FIFO_2);

        // if we get here, we have no open connections, so...
        // open FIFO for reading incoming connections:
        printf("YAMSD: opening syscall FIFO at %s\n", SERVER_FIFO_1);
        fd_syscall = open(SERVER_FIFO_1, O_RDONLY);
        printf("YAMSD: opening comm-channel FIFO at %s\n", SERVER_FIFO_2);
        fd_commchannel = open(SERVER_FIFO_2, O_RDONLY);

        // keep reading from request pipeline until we get a CONNECT request
        // or we get 10 bad requests:
        int bad_requests = 0;
        do {
            printf("YAMSD: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
            bad_requests++;
        }
        while(syscall_code != SYSCALL_CONNECT && bad_requests == 10);
        
        if(syscall_code != SYSCALL_CONNECT)
        {
            printf("YAMSD: error -- too many bad connect requests\n");
            return -1;
        }
        
        // if we get here, we have received a CONNECT request, so...
        // connect to our first process:
        connect_process(&(clients[nextPID]));

        // go into loop to read and respond to client requests:
        while(connections > 0)
        {
            // set up some communication variables:
            int clientPID; // which client process we are currently communicating with
            int param_int, param_int_2; // several syscalls send integer parameters
            char param_string[STRING_SIZE]; // several syscalls send a string parameter
            char mailbox_name[STRING_SIZE]; // the SEND syscall also identifies a target mailbox
            char response_string[STRING_SIZE*2]; // this is the response we echo back to the client process
            int response_int;
            
            // read a system call from the FIFO:
            printf("YAMSD: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
            
            if(syscall_code != SYSCALL_CONNECT) {
                // if this is not a new process connecting, 
                // we also need to read the process' PID:
                printf("YAMSD: reading client PID...");
                read_int(fd_syscall, &clientPID); 
                printf("read PID %d\n", clientPID); 
                // and then we need to issue the client a "lock" for the 
                // comm-channel FIFO for sending subsequent parameters;
                // this is simply done by echoing the client PID:
                printf("YAMSD: issuing lock to client %d to complete syscall %03o\n", clientPID, syscall_code);
                write_int(clients[clientPID].fd_outgoing, &clientPID);
            }
            else
            {
                // if there are any available slots, clients[nextPID].PID will equal the UNUSED flag
                if(clients[nextPID].PID == UNUSED)
                    connect_process(&(clients[nextPID]));
                else
                    // otherwise, handle the failure gracefully:
                    connect_fail();
            }
            

            if(clientPID < LIST_SIZE && clients[clientPID].PID != UNUSED)
            {
                switch(syscall_code)
                {
                case SYSCALL_CONNECT:
                    // nothing to do here
                    break;
                case SYSCALL_SHUTDOWN:
                    printf("YAMSD: received shutdown request\n");
                    // if there is only one connection, we can safely shut down
                    if(connections == 1)
                    {
                        printf("YAMSD: disconnecting last client and shutting down process server\n");
                        write_string(clients[clientPID].fd_outgoing, "SHUTTING DOWN. Goodbye.");
                        close(clients[clientPID].fd_outgoing);
                        connections = 0;
                        running = false;
                    }
                    else
                        // otherwise, we just disconnect the client process.
                        disconnect_process(&(clients[clientPID]));
                    break;
                case SYSCALL_EXIT:
                    disconnect_process(&(clients[clientPID]));
                    break;
                case SYSCALL_PING:
                    // syscall PING has one parameter: the integer code that we are to
                    // "bounce" back to the client
                    read_int(fd_commchannel, &param_int);
                    printf("YAMSD: received ping from process %d with code %d\n", clientPID, param_int);
                    sprintf(response_string, "Received PING with code %d", param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    break;
                case SYSCALL_CONFIGURE:
                    printf("YAMSD: received CONFIGURE request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CONFIGURE has n + 1 parameters, where n is the first byte after the syscall
                    read_int(fd_commchannel, &param_int);
                    printf("YAMSD: receiving %d configuration strings...\n", param_int);
                    sprintf(response_string, "Received CONFIGURE request for mailbox %s with %d configuration strings", clients[clientPID].mailbox_name, param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    for(int i = 0; i < param_int; i++)
                    {
                        read_string(fd_commchannel, param_string, STRING_SIZE);
                        printf("YAMSD: configuring %s.\n", param_string);
                        sprintf(response_string, "Configuring %s", param_string);
                        write_string(clients[clientPID].fd_outgoing, response_string);
                    }
                    break;
                case SYSCALL_SEND:
                    receive_message(clientPID);
                    break;
                case SYSCALL_CHECK:
                    check_messages(clientPID);
                    break;
                case SYSCALL_RECV:
                    fetch_message(clientPID);
                    break;
                case SYSCALL_GETPID:
                    // look up process PID:
                    response_int = clients[clientPID].PID;
                    printf("YAMSD: received GETPID request from process %d; returning value %d\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                    break;
                case SYSCALL_GETAGE:
                    // determine process age:
                    response_int = time(NULL) - clients[clientPID].start_time;
                    printf("YAMSD: received GETAGE request from process %d; process has been alive %d seconds\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                    break;
                case SYSCALL_JOINPID:
                    // syscall JOINPID has one parameter: the PID of the process to "join"
                    read_int(fd_commchannel, &param_int);
                    // only proceed if the specified PID is a "live" process:
                    if(clients[param_int].PID != UNUSED)
                    {
                        printf("YAMSD: received request from process %d to JOIN process %d\n", clientPID, param_int);
                        clients[clientPID].join_PID = param_int;
                    }
                    else
                    {
                        printf("YAMSD: received request from process %d to JOIN invalid process ID %d\n", clientPID, param_int);
                        response_int = -1;
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                    break;
                case SYSCALL_WAIT:
                    // syscall WAIT has one parameter: the PID of the process 
                    // to "wait" for a signal from:
                    read_int(fd_commchannel, &param_int);
                    // only proceed if the specified PID is a "live" process:
                    if(clients[param_int].PID != UNUSED)
                    {
                        printf("YAMSD: received request from process %d to WAIT for SIGNAL from process %d\n", clientPID, param_int);
                        clients[clientPID].wait_PID = param_int;
                    }
                    else
                    {
                        printf("YAMSD: received request from process %d to WAIT on invalid process ID %d\n", clientPID, param_int);
                        response_int = -1;
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                    break;
                case SYSCALL_SIGNAL:
                    // syscall SIGNAL has one parameter: the PID of the process 
                    // to send a "signal" to:
                    read_int(fd_commchannel, &param_int);
                    // only proceed if the specified PID is actually waiting for a signal from this client:
                    if(clients[param_int].wait_PID == clientPID)
                    {
                        printf("YAMSD: received SIGNAL from process %d to WAITing process %d\n", clientPID, param_int);
                        // clear the wait_PID for the WAITing client:
                        clients[param_int].wait_PID = UNUSED;
                        // send success (0) signals back to both clients:
                        response_int = 0;
                        write_int(clients[param_int].fd_outgoing, &response_int);
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                    else
                    {
                        printf("YAMSD: received request from process %d to SIGNAL non-waiting process ID %d\n", clientPID, param_int);
                        response_int = -1;
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                    break;
                default:
                    printf("YAMSD: received unknown system call %03o from process %d\n", syscall_code, clientPID);
                    sprintf(response_string, "Received unknown system call %o", syscall_code);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                }
            }
            else
            {
                printf("YAMSD: received request from invalid process ID number %d\n", clientPID);
            }
        }

        // if we reach this point there are no current connections, 
        // input will be undefined, so we need to close and re-open server FIFOs
        printf("YAMSD: resetting FIFO communication channels\n");
        close(fd_syscall);
        close(fd_commchannel);
        unlink(SERVER_FIFO_1);
        unlink(SERVER_FIFO_2);
    }

    return 0;
}