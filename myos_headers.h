#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> 
#include <string.h> 

/* ------------------------------------------------------------------ *
 * ------- myOS and myProcess: a simple process communication ------- *
 * -------           and synchronization system               ------- *
 * ------------------------------------------------------------------ *
 * This project defines a simple but versatile process commmunication *
 * and synchronization system; the process server can accept and      *
 * deliver messages between client processes for IPC purposes, and    *
 * can also provide several process synchronization services.         *
 *                                                                    *
 * The system uses a single server FIFO thru which the server accepts *
 * requests and system calls and a dedicated client FIFO for each     *
 * client process, using the client's host-OS PID as a naming key.    *
 * Each client process is assigned a unique PID for purposes of       *
 * identifying it within the set of client processes, and also as an  *
 * index into the array of current client records. Each client also   *
 * provides an IPC mailbox name as a C-string of up to 255 characters.*/

/* ------------------ DEFINE FIFO FILES HERE ------------------       */
#define SERVER_FIFO "myOS_fifo"
#define CLIENT_FIFO "myProcess_%d_fifo"
#define FIFO_MODE 0644

/* ---------------- DEFINE SOME STANDARD SIZES -----------------       */
#define STRING_SIZE 256
#define MAX_CLIENTS 100

/* ----------------- DEFINE SYSTEM CALLS HERE ------------------       */

/*  octal codes starting with 0 are for connection and configuration   */

/* CONNECT initiates new connection; it takes two parameters:          *
 * - int: PID of the client process                                    *
 * - C-string: mailbox name.                                           */
#define SYSCALL_CONNECT 000
/* PING checks connection status by "bouncing" a one-byte              *
 * "packet" off the server; it takes one parameter:                    *
 * - int: arbitrary number that is "bounced" back to the client        */
#define SYSCALL_PING 001
/* CONFIGURE sets mailbox parameters -- the user sets as many          *
 * C-string key-value pairs as they like; takes these param's:         *
 * - int: number of parameters to set                                  *
 * - (n) C-strings with format "key:value"                             */
#define SYSCALL_CONFIGURE 003
/* DISCONNECT closes client FIFO only; it takes one parameter:         *
 * - int: PID of the client process                                    */
#define SYSCALL_DISCONNECT 006
/* SHUTDOWN closes all FIFO's and ends server process; no params       */
#define SYSCALL_SHUTDOWN 007

/* octal codes starting with 1 are for interprocess messaging          */

/* SEND sends a message to a mailbox; it takes these parameters:       *
 * - C-string: destination mailbox name                                *
 * - int: number of strings to follow                                  *
 * - (n) C-strings: the message, as enumerated by previous byte        */
#define SYSCALL_SEND 010
/* CHECK queries the server to find out how many C-strings are         *
 * waiting in its mailbox; no param's                                  */
#define SYSCALL_CHECK 011
/* FETCH gets the first C-string waiting in the named mailbox;         *
 * no param's                                                          */
#define SYSCALL_FETCH 012