#include "myos_headers.h"
#include "msg_handlers.h"

/* ---------- define key communication variables ---------- */
int fd_incoming, fd_outgoing; // file descriptors for communication FIFO's
int my_linux_PID, my_client_PID; // host OS and process server PID's
char client_fifo_name[STRING_SIZE]; // filename for our client FIFO
char response[STRING_SIZE * 2]; // server response string
    
void send(int syscall_code)
{
    // send system call and PID to process server:
    printf("<- Sending syscall %03o to process server\n", syscall_code);
    write_int(fd_outgoing, &syscall_code);
    write_int(fd_outgoing, &my_client_PID);
}

void read_and_echo()
{
    // read and echo server response:
    read_string(fd_incoming, response, STRING_SIZE * 2);
    printf("-> Server sent response: %s\n", response);
}

int main()
{
    // greet the user
    printf("-~= Welcome to my process communication server! =~-\n");
    
    // ask user to specify mailbox name:
    char mailbox_name[STRING_SIZE];
    printf("Enter your mailbox name with no spaces: ");
    scanf("%s", mailbox_name);
    
    // open FIFO for reading incoming connections:
    printf("CLIENT: opening server FIFO at %s\n", SERVER_FIFO);
    fd_outgoing = open(SERVER_FIFO, O_WRONLY);

    // get PID for sending to server:
    my_linux_PID = getpid();
    srand(my_linux_PID);

    // modify client FIFO name with PID and create FIFO:
    sprintf(client_fifo_name, CLIENT_FIFO, my_linux_PID);
    mkfifo(client_fifo_name, FIFO_MODE);

    /* send CONNECT syscall                         *
     * parameters: int: PID, C-string: mailbox name */
    printf("myProcess: logging into process server\n");
    int syscall_code = SYSCALL_CONNECT;
    write(fd_outgoing, &syscall_code, sizeof(int));
    write(fd_outgoing, &my_linux_PID, sizeof(int));
    write_string(fd_outgoing, mailbox_name);
    
    // open FIFO for reading incoming connections:
    printf("myProcess: creating and opening client FIFO at %s\n", client_fifo_name);
    fd_incoming = open(client_fifo_name, O_RDONLY);

    // read and report server connection:
    read_int(fd_incoming, &my_client_PID);
    printf("myProcess: process server confirmed connection and gave me PID #%d.\n", my_client_PID);

    // now that server is connected, go into input-action loop:
    while (syscall_code != SYSCALL_DISCONNECT && syscall_code != SYSCALL_SHUTDOWN)
    {
        // enter interactive mode by printing a menu:
        printf("\n---------------------------------------------------------------------\n");
        printf("Enter a system call (%d = ping server, %d = configure mailbox,\n", SYSCALL_PING, SYSCALL_CONFIGURE);
        printf("%d = send message, %d = check for messages, %d = fetch first message,\n", SYSCALL_SEND, SYSCALL_CHECK, SYSCALL_FETCH);
        printf("%d = disconnect and exit, %d = kill server and exit): ", SYSCALL_DISCONNECT, SYSCALL_SHUTDOWN);
        // read user's choice:
        scanf("%d", &syscall_code);

        // send system call and PID to process server:
        send(syscall_code);

        // set up some more communication variables:
        char key[STRING_SIZE/2], value[STRING_SIZE/2]; // key-value pairs for CONFIGURE syscall
        char send_string[STRING_SIZE]; // several syscalls require sending a string
        int send_int; // several syscalls send an integer parameter

        // now respond to the user's choice more specifically:
        switch(syscall_code)
        {
            case SYSCALL_SHUTDOWN:
                /* send syscall SHUTDOWN *
                 * no parameters         */
                printf("Killing server and quitting client. Good-bye!\n");
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_DISCONNECT:
                /* send syscall DISCONNECT *
                 * no parameters           */
                printf("Disconnecting from server and quitting client. Good-bye!\n");
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_PING:
                /* send syscall PING                         *
                 * one parameter: int (here chosen randomly) */
                send_int = rand();
                printf("<- Sending ping with code %d\n", send_int);
                write_int(fd_outgoing, &send_int);
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_CONFIGURE:
                // ask user to specify number of settings:
                printf("How many settings do you want to configure? ");
                scanf("%d", &send_int);
                /* send syscall CONFIGURE                 *
                 * parameters: int: # of settings,        *
                 * list of C-strings: settings themselves */ 
                printf("<- Sending CONFIGURE %d request to server\n", send_int);
                write_int(fd_outgoing, &send_int);
                // read and echo server response:
                read_and_echo();
                // now handle the individual settings:
                for(int i = 1; i <= send_int; i++)
                {
                    // prompt for the setting name (key):
                    printf("key #%d: ", i);
                    scanf("%s", key);
                    // prompt for the setting value:
                    printf("value #%d: ", i);
                    scanf("%s", value);
                    // build and send a send-string:
                    sprintf(send_string, "%s:%s", key, value);
                    printf("<- Configuring %s\n", send_string);
                    write_string(fd_outgoing, send_string);
                    // read and echo server response:
                    read_and_echo();
                }
                printf("Sent %d settings to server\n", send_int);
                break; 
            case SYSCALL_SEND:
                // ask user to specify destination mailbox and number of lines to send:
                printf("Enter name of mailbox to send to: ");
                scanf("%s", send_string);
                printf("Send how many lines (one message per line)? ");
                scanf("%d", &send_int);
                /* send syscall SEND                      *
                 * parameters: C-string: mailbox,         *
                 * byte: number of C-strings to send      *
                 * list of C-strings: messages themselves */ 
                printf("Sending SEND %s:%d request to server\n", send_string, send_int);
                write_string(fd_outgoing, send_string);
                write_int(fd_outgoing, &send_int);
                // read and echo server response:
                read_and_echo();
                // clear the trailing newline character from the input buffer:
                fgets(send_string, STRING_SIZE, stdin);
                for(int i = 1; i <= send_int; i++)
                {
                    // prompt for the message string:
                    printf("msg #%d: ", i);
                    fgets(send_string, STRING_SIZE, stdin);
                    // strip trailing newline character:
                    send_string[strlen(send_string)-1] = '\0';
                    printf("<- sending message #%d\n", i);
                    write_string(fd_outgoing, send_string);
                    // read and echo server response:
                    read_and_echo();
                }
                printf("Sent %d messages to server\n", send_int);
                break;
            case SYSCALL_CHECK:
                /* send syscall CHECK                      *
                 * no parameters                           */
                printf("<- Sent CHECK request to server\n");
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_FETCH:
                /* send syscall FETCH                      *
                 * no parameters                           */
                printf("<- Sent FETCH request to server\n");
                // read and echo server response:
                read_and_echo();
                break;
            default:
                printf("%d is not a valid system call\n", syscall_code);
                // read and echo server response:
                read_and_echo();
        }
        
    }

    /* clean up our files on the way out */
    close(fd_outgoing);
    close(fd_incoming);
    unlink(client_fifo_name);

    return 0;
}