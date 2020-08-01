#include "yams_headers.h"
#include "fio_handlers.h"
#include "ipc_messaging.h"

/* ---------- define key communication variables ---------- */
int fd_incoming, fd_syscall, fd_commchannel; // file descriptors for communication FIFO's
int my_linux_PID, my_client_PID; // host OS and process server PID's
char client_fifo_name[STRING_SIZE]; // filename for our client FIFO
char response_string[STRING_SIZE * 2]; // server response string
int response_int; // server response integer

/* this function sends a system call to the process server  */    
void send(int syscall_code)
{
    // send system call and PID to process server:
    printf("<- Sending syscall %03o to process server\n", syscall_code);
    write_int(fd_syscall, &syscall_code);
    write_int(fd_syscall, &my_client_PID);
    // wait for the server to issue a "lock" before we proceed:
    read_int(fd_incoming, &response_int);
}

/* this function reads a response string from the IPC       *
 * server and echoes the response to the console            */
void read_string_and_echo()
{
    // read and echo server response:
    read_string(fd_incoming, response_string, STRING_SIZE * 2);
    printf("-> Server sent response: %s\n", response_string);
}

/* this function reads an integer response code from the    *
 * IPC server and echoes the response to the console        */
void read_int_and_echo()
{
    // read and echo server response:
    read_int(fd_incoming, &response_int);
    printf("-> Server sent response: %d\n", response_int);
}

/* this function reads input from the user and sends it as  *
 * a message to the IPC server                              */
void send_message()
{
    /* syscall SEND                             *
     * parameters: C-string: mailbox,           *
     * int: priority                            *
     * int: message type                        *
     * int: number of C-strings to send         *
     * list of C-strings: messages themselves   */ 

    char mbox_name[STRING_SIZE];
    char input;
    int priority, type, lines;
    // ask user to specify destination mailbox:
    printf("Enter name of mailbox to send to: ");
    scanf("%s", mbox_name);
    // enter a data validation loop for message priority:
    bool bad_data = true;
    while(bad_data)
    {
        printf("Message priority [(B)ATCH, (N)ORMAL, (I)NTERRUPT]? ");
        //clear residual newline character:
        scanf("%c", &input);
        //now get actual data:
        scanf("%c", &input);
        bad_data = false;
        switch (input)
        {
        case 'b':
        case 'B':
            priority = PRIORITY_BATCH;
            break;
        case 'n':
        case 'N':
            priority = PRIORITY_NORMAL;
            break;
        case 'i':
        case 'I':
            priority = PRIORITY_INTERRUPT;
            break;
        default:
            printf("'%c' was not one of the available choices, please try again.\n", input);
            bad_data = true;
            break;
        }
    }
    // enter a data validation loop for message type:
    bad_data = true;
    while(bad_data)
    {
        printf("Message type? [(I)NFO, RE(Q)UEST, (S)TATUS, (R)ESULT] ");
        //clear residual newline character:
        scanf("%c", &input);
        //now get actual data:
        scanf("%c", &input);
        bad_data = false;
        switch (input)
        {
        case 'i':
        case 'I':
            type = TYPE_INFO;
            break;
        case 'q':
        case 'Q':
            type = TYPE_REQUEST;
            break;
        case 's':
        case 'S':
            type = TYPE_STATUS;
            break;
        case 'r':
        case 'R':
            type = TYPE_RESULT;
            break;
        default:
            printf("That was not one of the available choices, please try again.\n");
            bad_data = true;
            break;
        }
    }
    // send what we have of the sys call params so far:
    printf("<- sending message to: %s with priority %d and type %d\n", mbox_name, priority, type);
    write_string(fd_commchannel, mbox_name);
    write_int(fd_commchannel, &priority);
    write_int(fd_commchannel, &type);
    // read and echo server response:
    read_string_and_echo();
    
    printf("Now enter your message, one line at a time, blank line to end:\n");
    lines = 0;
    char send_string[STRING_SIZE];
    // clear the trailing newline character from the input buffer:
    fgets(send_string, STRING_SIZE, stdin);
    do {
        printf("LINE %d: ", ++lines);
        fgets(send_string, STRING_SIZE, stdin);
        // strip trailing newline character:
        send_string[strlen(send_string)-1] = '\0';
        write_string(fd_commchannel, send_string);
    } while(strlen(send_string) > 0);
    // we actually over-count lines by one because of the
    // last empty line, so...
    lines--;
    printf("Sent %d message lines to server\n", lines);
    read_string_and_echo();            
}

void check_messages()
{
    /* send syscall CONFIGURE                       *
     * parameters:                                  *
     * int: priority level                          *
     * int: message type                            */
    char input;
    int priority, type;
    char sender[STRING_SIZE];
    
    printf("Check for messages of what priority [(S)PAM, (B)ATCH, (N)ORMAL, (I)NTERRUPT, (A)LL]? ");
    //clear residual newline character:
    scanf("%c", &input);
    //now get actual data:
    scanf("%c", &input);
    switch (input)
    {
    case 's':
    case 'S':
        priority = PRIORITY_SPAM;
        break;
    case 'b':
    case 'B':
        priority = PRIORITY_BATCH;
        break;
    case 'n':
    case 'N':
        priority = PRIORITY_NORMAL;
        break;
    case 'i':
    case 'I':
        priority = PRIORITY_INTERRUPT;
        break;
    default:
        priority = PRIORITY_ALL;
        break;
    }
    //send requested priority to server:
    write_int(fd_commchannel, &priority);
    
    printf("Check for messages of what type [(I)NFO, RE(Q)UEST, (S)TATUS, (R)ESULT, (A)LL]? ");
    //clear residual newline character:
    scanf("%c", &input);
    //now get actual data:
    scanf("%c", &input);
    switch (input)
    {
    case 'i':
    case 'I':
        type = TYPE_INFO;
        break;        
    case 'q':
    case 'Q':
        type = TYPE_REQUEST;
        break;
    case 's':
    case 'S':
        type = TYPE_STATUS;
        break;
    case 'r':
    case 'R':
        type = TYPE_RESULT;
        break;
    default:
        type = TYPE_ALL;
        break;
    }
    //send requested message type to server:
    write_int(fd_commchannel, &type);

    printf("Check for messages from what sender mailbox [type '*' for all]? ");
    scanf("%s", sender);
    write_string(fd_commchannel, sender);

    printf("<- Sent CHECK(%d, %d, %s) request to server\n", priority, type, sender);
    // read and echo server response:
    read_string_and_echo();
}

void fetch_message()
{
    /* send syscall FETCH                           *
     * parameters:                                  *
     * int: priority level                          *
     * int: message type                            */
    char input;
    int priority, type;
    char sender[STRING_SIZE];
    
    printf("Receive message of what priority [(S)PAM, (B)ATCH, (N)ORMAL, (I)NTERRUPT, (A)LL]? ");
    //clear residual newline character:
    scanf("%c", &input);
    //now get actual data:
    scanf("%c", &input);
    switch (input)
    {
    case 's':
    case 'S':
        priority = PRIORITY_SPAM;
        break;
    case 'b':
    case 'B':
        priority = PRIORITY_BATCH;
        break;
    case 'n':
    case 'N':
        priority = PRIORITY_NORMAL;
        break;
    case 'i':
    case 'I':
        priority = PRIORITY_INTERRUPT;
        break;
    default:
        priority = PRIORITY_ALL;
        break;
    }
    //send requested priority to server:
    write_int(fd_commchannel, &priority);
    
    printf("Receive message of what type? [(I)NFO, RE(Q)UEST, (S)TATUS, (R)ESULT, (A)LL] ");
    //clear residual newline character:
    scanf("%c", &input);
    //now get actual data:
    scanf("%c", &input);
    switch (input)
    {
    case 'i':
    case 'I':
        type = TYPE_INFO;
        break;        
    case 'q':
    case 'Q':
        type = TYPE_REQUEST;
        break;
    case 's':
    case 'S':
        type = TYPE_STATUS;
        break;
    case 'r':
    case 'R':
        type = TYPE_RESULT;
        break;
    default:
        type = TYPE_ALL;
        break;
    }
    //send requested message type to server:
    write_int(fd_commchannel, &type);

    printf("Receive message from what sender mailbox [type '*' for all]? ");
    scanf("%s", sender);
    write_string(fd_commchannel, sender);


    printf("<- Sent FETCH(%d, %d, %s) request to server\n", priority, type, sender);

    /* response takes the following form                                    *
     * - int: priority                                                      *
     * - int: message type                                                  *
     * - C-string: sender mailbox name                                      *
     * - int: number of lines                                               *
     * - (n) C-strings: the message                                         */
    // read response from the server:
    int num_lines;
    char pri[SHORT_STRING], typ[SHORT_STRING];
    read_int(fd_incoming, &priority);
    pri_str(pri, priority);
    read_int(fd_incoming, &type);
    typ_str(typ, type);
    read_string(fd_incoming, sender, STRING_SIZE);
    read_int(fd_incoming, &num_lines);
    if (num_lines == 0)
    {
        printf("-> Your mailbox contained an empty message:\n");
        printf("of priority %s, type %s, from mailbox %s\n", pri, typ, sender);
    }
    else
    {
        printf("-> %d-line message follows:\n", num_lines);
        printf("====----\n");
        printf("PRIORITY: %s\n", pri);
        printf("TYPE: %s\n", typ);
        printf("SENDER: %s\n", sender);
        printf("====----\n");
        for (int i = 0; i < num_lines; i++)
        {
            char message_line[STRING_SIZE];
            read_string(fd_incoming, message_line, STRING_SIZE);
            printf("%s\n", message_line);
        }
        printf("====----\n");
    }
}

int main()
{
    // greet the user
    printf("-~= Welcome to Yet Another Messaging Service =~-\n");
    
    // ask user to specify mailbox name:
    char mailbox_name[STRING_SIZE];
    printf("Enter your mailbox name with no spaces: ");
    scanf("%s", mailbox_name);
    
    // open FIFO for reading incoming connections:
    printf("YAMS client: opening syscall FIFO at %s\n", SERVER_FIFO_1);
    fd_syscall = open(SERVER_FIFO_1, O_WRONLY);
    printf("YAMS client: opening comm-channel FIFO at %s\n", SERVER_FIFO_1);
    fd_commchannel = open(SERVER_FIFO_2, O_WRONLY);

    // get PID for sending to server:
    my_linux_PID = getpid();
    srand(my_linux_PID);

    // modify client FIFO name with PID and create FIFO:
    sprintf(client_fifo_name, CLIENT_FIFO, my_linux_PID);
    mkfifo(client_fifo_name, FIFO_MODE);

    /* send CONNECT syscall                         *
     * parameters: int: PID, C-string: mailbox name */
    printf("YAMS client: logging into process server\n");
    int syscall_code = SYSCALL_CONNECT;
    write(fd_syscall, &syscall_code, sizeof(int));
    write(fd_syscall, &my_linux_PID, sizeof(int));
    write_string(fd_commchannel, mailbox_name);
    
    // open FIFO for reading incoming connections:
    printf("YAMS client: creating and opening client FIFO at %s\n", client_fifo_name);
    fd_incoming = open(client_fifo_name, O_RDONLY);

    // read and report server connection:
    read_int(fd_incoming, &my_client_PID);
    printf("YAMS client: process server confirmed connection and gave me PID #%d.\n", my_client_PID);

    // now that server is connected, go into input-action loop:
    while (syscall_code != SYSCALL_EXIT && syscall_code != SYSCALL_SHUTDOWN)
    {
        // enter interactive mode by printing a menu:
        printf("\n--------------------------------------------------------------------------------\n\n");
        printf("Enter a system call (%d = ping server, ", SYSCALL_PING);
        printf("%d = disconnect and exit, %d = kill server and exit,\n", SYSCALL_EXIT, SYSCALL_SHUTDOWN);
        printf("%d = send message, %d = check for messages, ", SYSCALL_SEND, SYSCALL_CHECK);
        printf("%d = fetch first message, %d = configure mailbox,\n", SYSCALL_RECV, SYSCALL_CONFIGURE);
        printf("%d = get PID, %d = get age, %d = join PID, ", SYSCALL_GETPID, SYSCALL_GETAGE, SYSCALL_JOINPID);
        printf("%d = wait PID, %d = signal PID): ", SYSCALL_WAIT, SYSCALL_SIGNAL);
        // read user's choice:
        scanf("%d", &syscall_code);
        printf("\n--------------------------------------------------------------------------------\n\n");
        
        // send system call and PID to process server:
        send(syscall_code);

        // set up some more communication variables:
        char key[STRING_SIZE/2], value[STRING_SIZE/2]; // key-value pairs for CONFIGURE syscall
        char send_string[STRING_SIZE]; // several syscalls require sending a string
        int send_int; // several syscalls send an integer parameter
        char send_char;
        bool bad_data = false;
        int response_int; // response code from server

        // now respond to the user's choice more specifically:
        switch(syscall_code)
        {
            case SYSCALL_SHUTDOWN:
                /* send syscall SHUTDOWN *
                 * no parameters         */
                printf("Killing server and quitting client. Good-bye!\n");
                // read and echo server response:
                read_string_and_echo();
                break;
            case SYSCALL_EXIT:
                /* send syscall DISCONNECT *
                 * no parameters           */
                printf("Disconnecting from server and quitting client. Good-bye!\n");
                // read and echo server response:
                read_string_and_echo();
                break;
            case SYSCALL_PING:
                /* send syscall PING                         *
                 * one parameter: int (here chosen randomly) */
                send_int = rand();
                printf("<- Sending ping with code %d\n", send_int);
                write_int(fd_commchannel, &send_int);
                // read and echo server response:
                read_string_and_echo();
                break;
            case SYSCALL_CONFIGURE:
                // ask user to specify number of settings:
                printf("How many settings do you want to configure? ");
                scanf("%d", &send_int);
                /* send syscall CONFIGURE                       *
                 * parameters:                                  *
                 * int: # of settings,                          *
                 * list of C-string pairs: settings themselves  */ 
                printf("<- Sending CONFIGURE %d request to server\n", send_int);
                write_int(fd_commchannel, &send_int);
                // read and echo server response:
                read_string_and_echo();
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
                    write_string(fd_commchannel, send_string);
                    // read and echo server response:
                    read_string_and_echo();
                }
                printf("Sent %d settings to server\n", send_int);
                break; 
            case SYSCALL_SEND:
                send_message();
                break;
            case SYSCALL_CHECK:
                check_messages();
                break;
            case SYSCALL_RECV:
                fetch_message();
                break;
            case SYSCALL_GETPID:
                /* send syscall GETPID                     *
                 * no parameters                           */
                printf("<- Sent GETPID request to server\n");
                read_int(fd_incoming, &response_int);
                printf("This process' PID is %d\n", response_int);
                break;
            case SYSCALL_GETAGE:
                /* send syscall GETPID                     *
                 * no parameters                           */
                printf("<- Sent GETAGE request to server\n");
                read_int(fd_incoming, &response_int);
                printf("This process' age is %d seconds\n", response_int);
                break;
            case SYSCALL_JOINPID:
                /* send syscall JOINPID                    *
                 * one parameter: int PID to join          */
                printf("What process ID do you want to JOIN? ");
                scanf("%d", &send_int);
                printf("<- Telling server to wake me up when process %d EXITs\n", send_int);
                write_int(fd_commchannel, &send_int);
                read_int(fd_incoming, &response_int);
                if(response_int < 0)
                    printf("-> Server returned an error: invalid PID\n");
                else
                    printf("-> Process %d has EXITed successfully\n", send_int);
                break;
            case SYSCALL_WAIT:
                /* send syscall WAIT                       *
                 * one parameter: int PIT to wait for      */
                printf("What process ID do you want to WAIT for a SIGNAL from? ");
                scanf("%d", &send_int);
                printf("<- Telling server to wake me up on a SIGNAL form process %d\n", send_int);
                write_int(fd_commchannel, &send_int);
                read_int(fd_incoming, &response_int);
                if(response_int < 0)
                    printf("-> Server returned an error: invalid PID\n");
                else
                    printf("-> Process %d has SIGNALed successfully\n", send_int);
                break;
            case SYSCALL_SIGNAL:
                /* send syscall SIGNAL                     *
                 * one parameter: int PID to signal to     */
                printf("What process ID do you want to send a SIGNAL to? ");
                scanf("%d", &send_int);
                printf("<- Telling server to SIGNAL process #%d\n", send_int);
                write_int(fd_commchannel, &send_int);
                read_int(fd_incoming, &response_int);
                if(response_int < 0)
                    printf("-> Server returned an error: specified process was not WAITing for a SIGNAL\n");
                else
                    printf("-> Process %d has received the SIGNAL successfully\n", send_int);
                break;
            default:
                printf("%d is not a valid system call\n", syscall_code);
                // read and echo server response:
                read_string_and_echo();
        }
    }

    /* clean up our files on the way out */
    close(fd_syscall);
    close(fd_commchannel);
    close(fd_incoming);
    unlink(client_fifo_name);

    return 0;
}