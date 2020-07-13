#include "myos_headers.h"
#include "msg_handlers.h"
#include <stdbool.h>
#include <time.h>

#define UNUSED -1

struct client {
    int PID;
    time_t start_time;
    char mailbox_name[STRING_SIZE];
    char fifo_name[STRING_SIZE];
    int fd_outgoing;
} clients[MAX_CLIENTS];

/* -----~~~~~===== define necessary global variables =====~~~~~----- */
int nextPID = 0;     // next available client process PID
int fd_syscall, fd_commchannel; // file pointers for incoming server FIFOs
int syscall_code;    // most recent incoming system call code
int connections = 0; // how many connected client processes
bool running = true; // whether the process server is supposed to
                     // still be running

/* the following function reads information from the server FIFO     *
 * to set up a new client struct and connect to a new client process */
void connectProcess(struct client *my_client)
{
    // CONNECT has two parameters -- int: linux PID, C-string: mailbox name
    int processLinuxPID;
    // read first parameter:
    read_int(fd_syscall, &processLinuxPID);
    // construct client FIFO name:
    sprintf(my_client->fifo_name, CLIENT_FIFO, processLinuxPID);
    // read mailbox name:
    read_string(fd_syscall, my_client->mailbox_name, STRING_SIZE);
    // give client process a new PID:
    printf("myOS: connecting Host-OS process #%d\n", processLinuxPID);
    my_client->PID = nextPID;
    nextPID = (nextPID + 1) % MAX_CLIENTS;
    // assign client process a start time:
    time(&(my_client->start_time));
    // report client connection:
    printf("myOS: client process #%d has connected with mailbox %s at time %s\n", my_client->PID, my_client->mailbox_name, ctime(&(my_client->start_time)));
    // open client FIFO:
    my_client->fd_outgoing = open(my_client->fifo_name, O_WRONLY);
    // report successful client connection:
    printf("myOS: opened client FIFO at %s\n", my_client->fifo_name);
    // send PID back to client to confirm connection:
    printf("myOS: sending PID %d to client\n", my_client->PID);
    write_int(my_client->fd_outgoing, &(my_client->PID));
    // note that we are now connected to one additional client process:
    connections++;
    printf("myOS: connected to %d clients\n", connections);
}

/* handle connection failure gracefully */
void connect_fail()
{
    int clientPID;
    char param_string[STRING_SIZE];
    // we need to flush the info from the server FIFO so we can
    // service the next system call:
    read_int(fd_syscall, &clientPID);
    printf("myOS: rejecting connection from Linux process %d -- too many clients connected", clientPID);
    read_string(fd_syscall, param_string, STRING_SIZE);
    printf("myOS: rejecting request to connect mailbox %s", param_string);
    // TODO: add code to connect client FIFO long enough to send an error code
}

/* the following function disconnects from a client process */
void disconnectProcess(struct client *my_client)
{
    printf("myOS: disconnecting from client %d.\n", my_client->PID);
    write_string(my_client->fd_outgoing, "DISCONNECTING. Goodbye.");
    close(my_client->fd_outgoing);
    my_client->PID = UNUSED;
    // note that we are now connected to one fewer client process:
    connections--;
    printf("myOS: connected to %d clients\n", connections);
}

int main()
{
    // just in case we need it, get my host OS PID:
    int my_linux_PID = getpid();
    // this is almost as arbitrary a number as any, so 
    // use it to seed my random number generator:
    srand(my_linux_PID);

    // initialize all client PID's to the unused flag:
    for(int i = 0; i < MAX_CLIENTS; i++)
        clients[i].PID = UNUSED;
    
    // "start up" the process server by creating 
    // named FIFO's for incoming connections:
    mkfifo(SERVER_FIFO_1, FIFO_MODE);
    printf("myOS: creating syscall FIFO at %s\n", SERVER_FIFO_1);
    mkfifo(SERVER_FIFO_2, FIFO_MODE);
    printf("myOS: creating comm-channel FIFO at %s\n", SERVER_FIFO_2);

    while(running)
    {
        // if we get here, we have no open connections, so...
        // open FIFO for reading incoming connections:
        printf("myOS: opening syscall FIFO at %s\n", SERVER_FIFO_1);
        fd_syscall = open(SERVER_FIFO_1, O_RDONLY);
        printf("myOS: opening comm-channel FIFO at %s\n", SERVER_FIFO_2);
        fd_commchannel = open(SERVER_FIFO_2, O_RDONLY);

        // keep reading from request pipeline until we get a CONNECT request:
        do {
            printf("myOS: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
        }
        while(syscall_code != SYSCALL_CONNECT);
        
        // if we get here, we have received a CONNECT request, so...
        // connect to our first process:
        connectProcess(&(clients[nextPID]));

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
            printf("myOS: attempting read from server FIFO...");
            read_int(fd_syscall, &syscall_code);
            printf("read syscall %03o\n", syscall_code);
            // if this is not a new process connecting, 
            // we also need to read the process' PID:
            if(syscall_code != SYSCALL_CONNECT) {
                printf("myOS: reading client PID...");
                read_int(fd_syscall, &clientPID); 
                printf("read PID %d\n", clientPID);  
            }

            if(clientPID < MAX_CLIENTS && clients[clientPID].PID != UNUSED)
            {
                switch(syscall_code)
                {
                case SYSCALL_CONNECT:
                    // if there are any available slots, clients[nextPID].PID will equal the UNUSED flag
                    if(clients[nextPID].PID == UNUSED)
                        connectProcess(&(clients[nextPID]));
                    else
                        // otherwise, handle the failure gracefully:
                        connect_fail();
                    break;
                case SYSCALL_SHUTDOWN:
                    printf("myOS: received shutdown request\n");
                    // if there is only one connection, we can safely shut down
                    if(connections == 1)
                    {
                        printf("myOS: disconnecting last client and shutting down process server\n");
                        write_string(clients[clientPID].fd_outgoing, "SHUTTING DOWN. Goodbye.");
                        close(clients[clientPID].fd_outgoing);
                        connections = 0;
                        running = false;
                    }
                    else
                        // otherwise, we just disconnect the client process.
                        disconnectProcess(&(clients[clientPID]));
                    break;
                case SYSCALL_EXIT:
                    disconnectProcess(&(clients[clientPID]));
                    break;
                case SYSCALL_PING:
                    // syscall PING has one parameter: the integer code that we are to
                    // "bounce" back to the client
                    read_int(fd_syscall, &param_int);
                    printf("myOS: received ping from process %d with code %d\n", clientPID, param_int);
                    sprintf(response_string, "Received PING with code %d", param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    break;
                case SYSCALL_CONFIGURE:
                    printf("myOS: received CONFIGURE request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CONFIGURE has n + 1 parameters, where n is the first byte after the syscall
                    read_int(fd_syscall, &param_int);
                    printf("myOS: receiving %d configuration strings...\n", param_int);
                    sprintf(response_string, "Received CONFIGURE request for mailbox %s with %d configuration strings", clients[clientPID].mailbox_name, param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    for(int i = 0; i < param_int; i++)
                    {
                        read_string(fd_commchannel, param_string, STRING_SIZE);
                        printf("myOS: configuring %s.\n", param_string);
                        sprintf(response_string, "Configuring %s", param_string);
                        write_string(clients[clientPID].fd_outgoing, response_string);
                    }
                    break;
                case SYSCALL_SEND:
                    // syscall SEND has a C-string mailbox name as first parameter:
                    read_string(fd_syscall, mailbox_name, STRING_SIZE);
                    // the next byte specifies the number of C-string messages to read:
                    read_int(fd_syscall, &param_int);
                    printf("myOS: received request from process %d to send %d messages to mailbox %s\n", clientPID, param_int, mailbox_name);
                    sprintf(response_string, "Received request to SEND %d messages to mailbox %s.\n", param_int, mailbox_name);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                    for(int i = 0; i < param_int; i++)
                    {
                        read_string(fd_commchannel, param_string, STRING_SIZE);
                        printf("myOS: message %d = \"%s\".\n", i+1, param_string);
                        sprintf(response_string, "Received message \"%s\"", param_string);
                        write_string(clients[clientPID].fd_outgoing, response_string);
                    }
                    break;
                case SYSCALL_CHECK:
                    printf("myOS: received CHECK request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CHECK is supposed to return the number of messages in the queue;
                    // for now, we will just return a random single-digit number:
                    param_int = (rand() % 10);
                    printf("myOS: pretending we have %d messages for mailbox %s\n", param_int, clients[clientPID].mailbox_name);
                    sprintf(response_string, "You have %d messages waiting", param_int);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                break;
                case SYSCALL_FETCH:
                    printf("myOS: received FETCH request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall FETCH is supposed to return the first message in the queue;
                    // for now, we will just return the string "DUMMY MESSAGE":
                    sprintf(response_string, "Message for you: \"%s\"", "DUMMY MESSAGE");
                    write_string(clients[clientPID].fd_outgoing, response_string);
                break;
                case SYSCALL_GETPID:
                    // look up process PID:
                    response_int = clients[clientPID].PID;
                    printf("myOS: received GETPID request from process %d; returning value %d\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                break;
                case SYSCALL_GETAGE:
                    // determine process age:
                    response_int = time(NULL) - clients[clientPID].start_time;
                    printf("myOS: received GETAGE request from process %d; process has been alive %d seconds\n", clientPID, response_int);
                    write_int(clients[clientPID].fd_outgoing, &response_int);
                break;
                default:
                    printf("myOS: received unknown system call %o from process %d\n", syscall_code, clientPID);
                    sprintf(response_string, "Received unknown system call %o", syscall_code);
                    write_string(clients[clientPID].fd_outgoing, response_string);
                }
            }
            else
            {
                printf("myOS: received request from invalid process ID number %d\n", clientPID);
            }
        }

        // if we reach this point there are no current connections, 
        // input will be undefined, so we need to close and re-open server FIFOs
        close(fd_syscall);
        close(fd_commchannel);
    }

    // if we reach this point we are done with our FIFO file, so...
    // delete the server FIFO:  
    unlink(SERVER_FIFO_1);
    unlink(SERVER_FIFO_2);

    return 0;
}