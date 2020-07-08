#include "myos_headers.h"
#include "string_handlers.h"
#include <stdbool.h>

#define UNUSED -1

struct client {
    int PID;
    char mailbox_name[STRING_SIZE];
    char fifo_name[STRING_SIZE];
    int fd_outgoing;
} clients[MAX_CLIENTS];

/* -----~~~~~===== define necessary global variables =====~~~~~----- */
int nextPID = 0;     // next available client process PID
int fd_incoming;     // file pointer for incoming server FIFO
int syscall_code;         // most recent incoming system call code
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
    read(fd_incoming, &processLinuxPID, sizeof(int));
    // construct client FIFO name:
    sprintf(my_client->fifo_name, CLIENT_FIFO, processLinuxPID);
    // read mailbox name:
    read_string(fd_incoming, my_client->mailbox_name, STRING_SIZE);
    // give client process a new PID:
    my_client->PID = nextPID;
    nextPID = (nextPID + 1) % MAX_CLIENTS;
    // report client connection:
    printf("myOS: host OS process #%d has connected with mailbox %s\n", processLinuxPID, my_client->mailbox_name);
    // open client FIFO:
    my_client->fd_outgoing = open(my_client->fifo_name, O_WRONLY);
    // report successful client connection:
    printf("myOS: opened client FIFO at %s\n", my_client->fifo_name);
    // note that we are now connected to one additional process:
    connections++;
    // send PID back to client to confirm connection:
    printf("myOS: sending PID %d to client\n", my_client->PID);
    write(my_client->fd_outgoing, my_client->fifo_name, sizeof(int));
}

/* the following function disconnects from a client process */
void disconnectProcess(struct client *my_client)
{
    printf("myOS: disconnecting from client %d.\n", my_client->PID);
    write(my_client->fd_outgoing, "DISCONNECTING. Goodbye.", 23);
    close(my_client->fd_outgoing);
    my_client->PID = UNUSED;
    connections--;
}

int main()
{
    // just in case we need it, get my host OS PID:
    int my_linux_PID = getpid();
    // this is as arbitrary a number as any, so 
    // use it to seed my random number generator:
    srand(my_linux_PID);

    // initialize all client PID's to the unused flag:
    for(int i = 0; i < MAX_CLIENTS; i++)
        clients[i].PID = UNUSED;
    
    // "start up" the process server by creating 
    // a named FIFO for incoming server connections:
    mkfifo(SERVER_FIFO, FIFO_MODE);
    printf("myOS: creating server FIFO at %s\n", SERVER_FIFO);

    while(running)
    {
        // open FIFO for reading incoming connections:
        printf("myOS: opening server FIFO at %s\n", SERVER_FIFO);
        fd_incoming = open(SERVER_FIFO, O_RDONLY);

        // keep reading from request pipeline until we get a CONNECT request:
        do
            read(fd_incoming, &syscall_code, sizeof(int));
        while(syscall_code != SYSCALL_CONNECT);
        
        // connect to our first process 
        connectProcess(&(clients[nextPID]));

        // go into loop to read and respond to client requests:
        while(connections > 0)
        {
            read(fd_incoming, &syscall_code, sizeof(int));
            u_int8_t byte;
            int clientPID;
            char response[STRING_SIZE*2], config_string[STRING_SIZE], mailbox_name[STRING_SIZE], message_string[STRING_SIZE];
            if(syscall_code != SYSCALL_CONNECT)
                read(fd_incoming, &clientPID, sizeof(int));    

            switch(syscall_code)
            {
                case SYSCALL_CONNECT:
                    if(clients[nextPID].PID == UNUSED)
                        connectProcess(&(clients[nextPID]));
                    else
                        read(fd_incoming, &clientPID, sizeof(int));
                        printf("myOS: rejecting connection from Linux process %d -- too many clients connected", clientPID);
                        read_string(fd_incoming, message_string, STRING_SIZE);
                        printf("myOS: rejecting request to connect mailbox %s", message_string);
                    break;
                case SYSCALL_SHUTDOWN:
                    printf("myOS: received shutdown request\n");
                    if(connections == 1)
                    {
                        printf("myOS: disconnecting last client and shutting down process server\n");
                        write(clients[clientPID].fd_outgoing, "SHUTTING DOWN. Goodbye.", 24);
                        close(clients[clientPID].fd_outgoing);
                        connections = 0;
                        running = false;
                    }
                    else
                        disconnectProcess(&(clients[clientPID]));
                    break;
                case SYSCALL_DISCONNECT:
                    disconnectProcess(&(clients[clientPID]));
                    break;
                case SYSCALL_PING:
                    // syscall PING has one parameter: the byte code that we are to
                    // "bounce" back to the client
                    read(fd_incoming, &byte, 1);
                    printf("myOS: received ping from process %d with code %d\n", clientPID, byte);
                    sprintf(response, "Received PING with code %d", byte);
                    write_string(clients[clientPID].fd_outgoing, response);
                    break;
                case SYSCALL_CONFIGURE:
                    printf("myOS: received CONFIGURE request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CONFIGURE has n + 1 parameters, where n is the first byte after the syscall
                    read(fd_incoming, &byte, 1);
                    sprintf(response, "Received CONFIGURE request for mailbox %s with %d configuration strings", clients[clientPID].mailbox_name, byte);
                    write_string(clients[clientPID].fd_outgoing, response);
                    for(int i = 0; i < byte; i++)
                    {
                        read_string(fd_incoming, config_string, STRING_SIZE);
                        printf("myOS: configuring %s.\n", config_string);
                        sprintf(response, "Configuring %s", config_string);
                        write_string(clients[clientPID].fd_outgoing, response);
                    }
                    break;
                case SYSCALL_SEND:
                    // syscall SEND has a C-string mailbox name as first parameter:
                    read_string(fd_incoming, mailbox_name, STRING_SIZE);
                    // the next byte specifies the number of C-string messages to read:
                    read(fd_incoming, &byte, 1);
                    printf("myOS: received request from process %d to send %d messages to mailbox %s\n", clientPID, byte, mailbox_name);
                    sprintf(response, "Received request to SEND %d messages to mailbox %s.\n", byte, mailbox_name);
                    write_string(clients[clientPID].fd_outgoing, response);
                    for(int i = 0; i < byte; i++)
                    {
                        read_string(fd_incoming, message_string, STRING_SIZE);
                        printf("myOS: message %d = \"%s\".\n", i+1, message_string);
                        sprintf(response, "Received message \"%s\"", message_string);
                        write_string(clients[clientPID].fd_outgoing, response);
                    }
                    break;
                case SYSCALL_CHECK:
                    printf("myOS: received CHECK request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall CHECK is supposed to return the number of messages in the queue;
                    // for now, we will just return a random single-digit number:
                    byte = (u_int8_t)(rand() % 10);
                    sprintf(response, "You have %d messages waiting", byte);
                    write_string(clients[clientPID].fd_outgoing, response);
                break;
                case SYSCALL_FETCH:
                    printf("myOS: received FETCH request for mailbox %s\n", clients[clientPID].mailbox_name);
                    // syscall FETCH is supposed to return the first message in the queue;
                    // for now, we will just return the string "DUMMY MESSAGE":
                    sprintf(response, "Message for you: \"%s\"", "DUMMY MESSAGE");
                    write_string(clients[clientPID].fd_outgoing, response);
                break;
                default:
                    printf("myOS: received unknown system call %o from process %d\n", syscall_code, clientPID);
                    sprintf(response, "Received unknown system call %o", syscall_code);
                    write_string(clients[clientPID].fd_outgoing, response);
            }
        }
        // no current connections, input will be undefined, so we
        // need to close and re-open server FIFO
        close(fd_incoming);
    }    
    
    return 0;
}