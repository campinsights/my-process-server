#include "myos_headers.h"
#include "string_handlers.h"

// define FIFO communication variables:
int fd_incoming, fd_outgoing, my_linux_PID, my_client_PID;
char client_fifo_name[STRING_SIZE], response[STRING_SIZE * 2];
    
void send(int syscall_code)
{
    // send system call and PID to process server:
    printf("<- Sending syscall %o to process server\n", syscall);
    write(fd_outgoing, &syscall_code, sizeof(int));
    write(fd_outgoing, &my_client_PID, sizeof(int));
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

    /* send CONNECT syscall                       *
     * parameters: int: PID, C-string: mailbox name */
    printf("process: logging into process server\n");
    int syscall = SYSCALL_CONNECT;
    write(fd_outgoing, &syscall, sizeof(int));
    write(fd_outgoing, &my_linux_PID, sizeof(int));
    write_string(fd_outgoing, mailbox_name);
    
    // open FIFO for reading incoming connections:
    printf("process: creating and opening client FIFO at %s\n", client_fifo_name);
    fd_incoming = open(client_fifo_name, O_RDONLY);

    // read and report server connection:
    read(fd_incoming, &my_client_PID, sizeof(int));
    printf("process: server confirmed connection and gave me PID # %d.\n", my_client_PID);

    // now that server is connected, go into input-action loop:
    while (syscall != SYSCALL_DISCONNECT && syscall != SYSCALL_SHUTDOWN)
    {
        // enter interactive mode by printing a menu:
        printf("\n---------------------------------------------------------------------\n");
        printf("Enter a system call (%d = ping server, %d = configure mailbox,\n", SYSCALL_PING, SYSCALL_CONFIGURE);
        printf("%d = send message, %d = check for messages, %d = fetch first message,\n", SYSCALL_SEND, SYSCALL_CHECK, SYSCALL_FETCH);
        printf("%d = disconnect, %d = kill server): ", SYSCALL_DISCONNECT, SYSCALL_SHUTDOWN);
        // read user's choice:
        scanf("%d", &syscall);

        // send system call and PID to process server:
        send(syscall);

        // now respond to user's choice specifically:
        char key[STRING_SIZE/2], value[STRING_SIZE/2], send_string[STRING_SIZE];
        u_int8_t byte;
        int byte_input;
        switch(syscall)
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
                byte = (u_int8_t)rand();
                printf("<- Sending ping with code %d\n", byte);
                write(fd_outgoing, &syscall, sizeof(int));
                write(fd_outgoing, &byte, 1);
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_CONFIGURE:
                // ask user to specify number of settings:
                printf("How many settings do you want to configure? ");
                scanf("%d", &byte_input);
                byte = (u_int8_t)byte_input;
                /* send syscall CONFIGURE                 *
                 * parameters: byte: # of settings,       *
                 * list of C-strings: settings themselves */ 
                printf("<- Sending CONFIGURE request to server\n");
                write(fd_outgoing, &byte, 1);
                // read and echo server response:
                read_and_echo();
                for(int i = 1; i <= byte; i++)
                {
                    printf("key #%d: ", i);
                    scanf("%s", key);
                    printf("value #%d: ", i);
                    scanf("%s", value);
                    sprintf(send_string, "%s:%s", key, value);
                    write_string(fd_outgoing, send_string);
                    // read and echo server response:
                    read_string(fd_incoming, response, STRING_SIZE);
                    printf("-> Server sent response: %s\n", response);
                }
                printf("Sent %d settings to server\n", byte);
                break; 
            case SYSCALL_SEND:
                // ask user to specify destination mailbox and number of lines to send:
                printf("Enter name of mailbox to send to: ");
                scanf("%s", send_string);
                printf("Send how many lines (one message per line)? ");
                scanf("%d", &byte_input);
                byte = (u_int8_t)byte_input;
                /* send syscall SEND                      *
                 * parameters: C-string: mailbox,         *
                 * byte: number of C-strings to send      *
                 * list of C-strings: messages themselves */ 
                printf("<- Sending SEND request to server\n");
                write(fd_outgoing, &syscall, sizeof(int));
                write_string(fd_outgoing, send_string);
                write(fd_outgoing, &byte, 1);
                // read and echo server response:
                read_and_echo();
                // clear the trailing newline character from the input buffer:
                fgets(send_string, STRING_SIZE, stdin);
                for(int i = 1; i <= byte; i++)
                {
                    printf("msg #%d: ", i);
                    fgets(send_string, STRING_SIZE, stdin);
                    // strip trailing newline character
                    send_string[strlen(send_string)-1] = '\0';
                    write_string(fd_outgoing, send_string);
                    // read and echo server response:
                    read_string(fd_incoming, response, STRING_SIZE);
                    printf("-> Server sent response: %s\n", response);
                }
                printf("Sent %d messages to server\n", byte);
                break;
            case SYSCALL_CHECK:
                /* send syscall CHECK                      *
                 * no parameters                           */
                write(fd_outgoing, &syscall, sizeof(int));
                printf("<- Sent CHECK request to server\n");
                // read and echo server response:
                read_and_echo();
                break;
            case SYSCALL_FETCH:
                /* send syscall FETCH                      *
                 * no parameters                           */
                write(fd_outgoing, &syscall, sizeof(int));
                printf("<- Sent FETCH request to server\n");
                // read and echo server response:
                read_and_echo();
                break;
            default:
                printf("%d is not a valid system call\n", syscall);
                // read and echo server response:
                read_and_echo();
        }
        
    }

    /* clean up our files on the way out */
    close(fd_outgoing);
    close(fd_incoming);
    
    return 0;
}