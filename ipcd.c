#include "ipc_headers.h"
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
        printf("IPCD: new mailbox %s created at hash %d\n", mbox_name, hash);
        return mboxes[hash];
    }
    else
    {
        int list_posn = find_mbox(mboxes[hash], mbox_name);
        if(list_posn < 0)
        {
            int list_len = add_mbox(mboxes[hash], mbox_name);
            printf("IPCD: new mailbox %s added at hash %d to form a list of length %d\n", mbox_name, hash, list_len);
            // since list positions are zero-based, return mailbox at list_len - 1:
            return get_mbox_at(mboxes[hash], list_len - 1);
        }
        else
        {
            printf("IPCD: mailbox %s already registered at hash %d, position %d\n", mbox_name, hash, list_posn);
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
    printf("IPCD: connecting Host-OS process #%d on named pipe %s\n", processLinuxPID, my_client->fifo_name);
    // read mailbox name:
    read_string(fd_commchannel, my_client->mailbox_name, STRING_SIZE);
    struct Mailbox * mbox = register_mbox(my_client->mailbox_name);
    int waiting = num_waiting_msgs(mbox);
    printf("IPCD: mailbox %s registered successfully; it has %d waiting messages\n", my_client->mailbox_name, waiting);
    // give client process a new PID:
    my_client->PID = nextPID;
    nextPID = (nextPID + 1) % LIST_SIZE;
    // assign client process a start time:
    time(&(my_client->start_time));
    // report client connection:
    printf("IPCD: client process #%d has connected with mailbox %s at time %s\n", my_client->PID, my_client->mailbox_name, ctime(&(my_client->start_time)));
    // open client FIFO:
    my_client->fd_outgoing = open(my_client->fifo_name, O_WRONLY);
    // report successful client connection:
    printf("IPCD: opened client FIFO at %s\n", my_client->fifo_name);
    // send PID back to client to confirm connection:
    printf("IPCD: sending PID %d to client\n", my_client->PID);
    write_int(my_client->fd_outgoing, &(my_client->PID));
    // note that we are now connected to one additional client process:
    connections++;
    printf("IPCD: connected to %d clients\n", connections);
}

/* handle connection failure gracefully */
void connect_fail()
{
    int clientPID;
    char param_string[STRING_SIZE];
    // we need to flush the info from the server FIFO so we can
    // service the next system call:
    read_int(fd_syscall, &clientPID);
    printf("IPCD: rejecting connection from Linux process %d -- too many clients connected", clientPID);
    read_string(fd_syscall, param_string, STRING_SIZE);
    printf("IPCD: rejecting request to connect mailbox %s", param_string);
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
    printf("IPCD: disconnecting from client %d.\n", my_client->PID);
    write_string(my_client->fd_outgoing, "DISCONNECTING. Goodbye.");
    close(my_client->fd_outgoing);
    my_client->PID = UNUSED;
    my_client->fd_outgoing = UNUSED;
    // note that we are now connected to one fewer client process:
    connections--;
    printf("IPCD: connected to %d clients\n", connections);
}

/* the following function receives a client message             */
void receive_message(int clientPID)
{
    /* syscall SEND                             *
     * parameters: C-string: mailbox,           *
     * int: priority                            *
     * int: message type                        *
     * int: number of C-strings to send         *
     * list of C-strings: messages themselves   */ 
    
    // read the mailbox name:
    char mbox_name[STRING_SIZE];
    read_string(fd_commchannel, mbox_name, STRING_SIZE);

    // read the message priority and type:
    int priority;
    read_int(fd_commchannel, &priority);
    int type;
    read_int(fd_commchannel, &type);
    printf("IPCD: receiving priority %d, type %d message from cliend %d for mailbox %s\n", priority, type, clientPID, mbox_name);

    // find the mailbox, create a new message here

    char response_string[STRING_SIZE*2];
    sprintf(response_string, "Ready to receive priority %d, type %d message for mailbox %s", priority, type, mbox_name);
    write_string(clients[clientPID].fd_outgoing, response_string);
    
    char message_line[STRING_SIZE];
    int lines = 0;
    do {
        read_string(fd_commchannel, message_line, STRING_SIZE);
        printf("IPCD: received line %d = '%s'\n", ++lines, message_line);
        
        // add message_line to message here
    } while(strlen(message_line) > 0);
    // we actually over-count lines by one because of the
    // last empty line, so...
    lines--;
    // client expects a confirmation, so...
    sprintf(response_string, "Received %d message lines", lines);
    write_string(clients[clientPID].fd_outgoing, response_string);
    printf("IPCD: finished receiving %d lines of text\n", lines);
}

int main()
{
    // just in case we need it, get my host OS PID:
    int my_linux_PID = getpid();
    // this is almost as arbitrary a number as any, so 
    // use it to seed my random number generator:
    srand(my_linux_PID);

    // initialize all client PID's and mailboxes:
    for(int i = 0; i < LIST_SIZE; i++)
    {
        clients[i].PID = UNUSED;
        clients[i].join_PID = UNUSED;
        clients[i].wait_PID = UNUSED;
        clients[i].fd_outgoing = UNUSED;
        mboxes[i] = NULL;
    }

    while(running)
    {
        // "start up" the process server by creating 
        // named FIFO's for incoming connections:
        mkfifo(SERVER_FIFO_1, FIFO_MODE);
        printf("IPCD: creating syscall FIFO at %s\n", SERVER_FIFO_1);
        mkfifo(SERVER_FIFO_2, FIFO_MODE);
        printf("IPCD: creating comm-channel FIFO at %s\n", SERVER_FIFO_2);

        // if we get here, we have no open connections, so...
        // open FIFO for reading incoming connections:
        printf("IPCD: opening syscall FIFO at %s\n", SERVER_FIFO_1);
        fd_syscall = open(SERVER_FIFO_1, O_RDONLY);
        printf("IPCD: opening comm-channel FIFO at %s\n", SERVER_FIFO_2);
        fd_commchannel = open(SERVER_FIFO_2, O_RDONLY);

        // keep reading from request pipeline until we get a CONNECT request
        // or we get 10 bad requests:
        int bad_requests = 0;
        do {
            printf("IPCD: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
            bad_requests++;
        }
        while(syscall_code != SYSCALL_CONNECT && bad_requests == 10);
        
        if(syscall_code != SYSCALL_CONNECT)
        {
            printf("IPCD: error -- too many bad connect requests\n");
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
            int param_int; // several syscalls send an integer parameter
            char param_string[STRING_SIZE]; // several syscalls send a string parameter
            char mailbox_name[STRING_SIZE]; // the SEND syscall also identifies a target mailbox
            char response_string[STRING_SIZE*2]; // this is the response we echo back to the client process
            int response_int;
            
            // read a system call from the FIFO:
            printf("IPCD: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
            
            if(syscall_code != SYSCALL_CONNECT) {
                // if this is not a new process connecting, 
                // we also need to read the process' PID:
                printf("IPCD: reading client PID...");
                read_int(fd_syscall, &clientPID); 
                printf("read PID %d\n", clientPID); 
                // and then we need to issue the client a "lock" for the 
                // comm-channel FIFO for sending subsequent parameters;
                // this is simply done by echoing the client PID:
                printf("IPCD: issuing lock to client %d to complete syscall %03o\n", clientPID, syscall_code);
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
                    printf("IPCD: received shutdown request\n");
                    // if there is only one connection, we can safely shut down
                    if(connections == 1)
                    {
                        printf("IPCD: disconnecting last client and shutting down process server\n");
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
                    printf("IPCD: received ping from process %d with code %d\n", clientPID, param_int);
                    sprintf(response_string, "Received PING with code %d", param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    break;
                case SYSCALL_CONFIGURE:
                    printf("IPCD: received CONFIGURE request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CONFIGURE has n + 1 parameters, where n is the first byte after the syscall
                    read_int(fd_commchannel, &param_int);
                    printf("IPCD: receiving %d configuration strings...\n", param_int);
                    sprintf(response_string, "Received CONFIGURE request for mailbox %s with %d configuration strings", clients[clientPID].mailbox_name, param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    for(int i = 0; i < param_int; i++)
                    {
                        read_string(fd_commchannel, param_string, STRING_SIZE);
                        printf("IPCD: configuring %s.\n", param_string);
                        sprintf(response_string, "Configuring %s", param_string);
                        write_string(clients[clientPID].fd_outgoing, response_string);
                    }
                    break;
                case SYSCALL_SEND:
                    receive_message(clientPID);
                    break;
                case SYSCALL_CHECK:
                    printf("IPCD: received CHECK request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CHECK is supposed to return the number of messages in the queue;
                    // for now, we will just return a random single-digit number:
                    param_int = (rand() % 10);
                    printf("IPCD: pretending we have %d messages for mailbox %s\n", param_int, clients[clientPID].mailbox_name);
                    sprintf(response_string, "You have %d messages waiting", param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                break;
                case SYSCALL_FETCH:
                    printf("IPCD: received FETCH request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall FETCH is supposed to return the first message in the queue;
                    // for now, we will just return the string "DUMMY MESSAGE":
                    sprintf(response_string, "Message for you: \"%s\"", "DUMMY MESSAGE");
                    write_string(clients[clientPID].fd_outgoing, response_string);
                break;
                case SYSCALL_GETPID:
                    // look up process PID:
                    response_int = clients[clientPID].PID;
                    printf("IPCD: received GETPID request from process %d; returning value %d\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                break;
                case SYSCALL_GETAGE:
                    // determine process age:
                    response_int = time(NULL) - clients[clientPID].start_time;
                    printf("IPCD: received GETAGE request from process %d; process has been alive %d seconds\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                break;
                case SYSCALL_JOINPID:
                    // syscall JOINPID has one parameter: the PID of the process to "join"
                    read_int(fd_commchannel, &param_int);
                    // only proceed if the specified PID is a "live" process:
                    if(clients[param_int].PID != UNUSED)
                    {
                        printf("IPCD: received request from process %d to JOIN process %d\n", clientPID, param_int);
                        clients[clientPID].join_PID = param_int;
                    }
                    else
                    {
                        printf("IPCD: received request from process %d to JOIN invalid process ID %d\n", clientPID, param_int);
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
                        printf("IPCD: received request from process %d to WAIT for SIGNAL from process %d\n", clientPID, param_int);
                        clients[clientPID].wait_PID = param_int;
                    }
                    else
                    {
                        printf("IPCD: received request from process %d to WAIT on invalid process ID %d\n", clientPID, param_int);
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
                        printf("IPCD: received SIGNAL from process %d to WAITing process %d\n", clientPID, param_int);
                        // clear the wait_PID for the WAITing client:
                        clients[param_int].wait_PID = UNUSED;
                        // send success (0) signals back to both clients:
                        response_int = 0;
                        write_int(clients[param_int].fd_outgoing, &response_int);
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                    else
                    {
                        printf("IPCD: received request from process %d to SIGNAL non-waiting process ID %d\n", clientPID, param_int);
                        response_int = -1;
                        write_int(clients[clientPID].fd_outgoing, &response_int);
                    }
                break;
                default:
                    printf("IPCD: received unknown system call %03o from process %d\n", syscall_code, clientPID);
                    sprintf(response_string, "Received unknown system call %o", syscall_code);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                }
            }
            else
            {
                printf("IPCD: received request from invalid process ID number %d\n", clientPID);
            }
        }

        // if we reach this point there are no current connections, 
        // input will be undefined, so we need to close and re-open server FIFOs
        printf("IPCD: resetting FIFO communication channels\n");
        close(fd_syscall);
        close(fd_commchannel);
        unlink(SERVER_FIFO_1);
        unlink(SERVER_FIFO_2);
    }

    return 0;
}