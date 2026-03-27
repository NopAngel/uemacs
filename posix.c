/*	posix.c
 *
 *      The functions in this file negotiate with the operating system for
 *      characters, and write characters in a barely buffered fashion on the
 *      display. All operating systems.
 *
 *	modified by Petri Kutvonen
 *
 *	based on termio.c, with all the old cruft removed, and
 *	fixed for termios rather than the old termio.. Linus Torvalds
 *  btw this is a FUCKING FORK by: NopAngel
 */


#ifdef POSIX

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <string.h>    

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

#if defined(__APPLE__) || defined(__FreeBSD__)
#ifndef OLCUC
#define OLCUC 0000002
#endif
#ifndef XCASE
#define XCASE 0000004
#endif
#endif

static struct termios otermios; 
static struct termios ntermios; 
static int kbdflgs;

#define TBUFSIZ 1024 
static char tobuf[TBUFSIZ];

void ttopen(void) 
{
    if (tcgetattr(STDIN_FILENO, &otermios) < 0) {
        perror("tcgetattr");
        exit(1);
    }

    ntermios = otermios;

    ntermios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    ntermios.c_oflag &= ~(OPOST);
    ntermios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    ntermios.c_cflag &= ~(CSIZE | PARENB);
    ntermios.c_cflag |= CS8;

    ntermios.c_cc[VMIN] = 1;
    ntermios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &ntermios) < 0) {
        perror("tcsetattr");
        exit(1);
    }

    setvbuf(stdout, tobuf, _IOFBF, TBUFSIZ);

    kbdflgs = fcntl(STDIN_FILENO, F_GETFL, 0);
    
    ttrow = 999;
    ttcol = 999;
}

void ttclose(void) 
{
    tcsetattr(STDIN_FILENO, TCSADRAIN, &otermios);
}

int ttputc(int c) 
{
    char utf8[6];
    int bytes = unicode_to_utf8(c, utf8);
    
    if (fwrite(utf8, 1, bytes, stdout) != (size_t)bytes) {
        return FALSE;
    }
    return TRUE;
}

void ttflush(void) 
{
    if (fflush(stdout) != 0) {
        if (errno == EAGAIN) {
            usleep(1000); 
            fflush(stdout);
        } else {
            exit(15);
        }
    }
}

int ttgetc(void) 
{
    static char buffer[64]; 
    static int pending = 0;
    unicode_t c;
    int bytes;

    if (pending <= 0) {
        pending = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (pending <= 0) return 0;
    }


    if ((unsigned char)buffer[0] < 128) {
        c = (unsigned char)buffer[0];
        bytes = 1;
        

        if (c == 27 && pending >= 2 && buffer[1] == '[') {
            c = 128 + 27;
            bytes = 2;
        }
    } else {

        bytes = utf8_to_unicode(buffer, 0, pending, &c);
        if (c == 0xa0) c = ' '; 
    }

    pending -= bytes;
    if (pending > 0) {
        memmove(buffer, buffer + bytes, pending);
    }
    
    return (int)c;
}

int typahead(void) 
{
    int x = 0;
    if (ioctl(STDIN_FILENO, FIONREAD, &x) < 0)
        return 0;
    return x;
}

#endif