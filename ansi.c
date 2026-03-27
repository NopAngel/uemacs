/* ANSI.C
 *
 * The routines in this file provide support for ANSI style terminals.
 * This is a specialized fork based on Petri Kutvonen's modification 
 * of Linus Torvalds' uEmacs version.
 *
 * Optimized for reduced syscall overhead and modern ANSI compliance.
 */

#define termdef 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "estruct.h"
#include "edef.h"

#if ANSI

/* Global Screen Constraints */
#define NROW    25      /* Default Screen rows */
#define NCOL    80      /* Default Screen columns */

#if PKCODE
#define MROW    64
#endif

#define NPAUSE  100 
#define MARGIN  8 
#define SCRSIZ  64 
#define BEL     0x07
#define ESC     0x1B

/* External Terminal IO (from posix.c / termio.c) */
extern int ttopen(void);
extern int ttgetc(void);
extern int ttputc(int c);
extern int ttflush(void);
extern int ttclose(void);

#if COLOR
static int cfcolor = -1; /* current foreground color */
static int cbcolor = -1; /* current background color */
#endif

/* Forward declarations for the terminal dispatch table */
void ansiopen(void);
void ansiclose(void);
void ansikopen(void);
void ansikclose(void);
int ansimove(int row, int col);
void ansieeol(void);
void ansieeop(void);
void ansibeep(void);
void ansirev(int state);
int ansicres(void);
void ansiparm(int n);

#if COLOR
void ansifcol(int color);
void ansibcol(int color);
#endif

/*
 * Standard terminal interface dispatch table.
 */
struct terminal term = {
#if PKCODE
    MROW - 1,
#else
    NROW - 1,
#endif
    NROW - 1,
    NCOL,
    NCOL,
    MARGIN,
    SCRSIZ,
    NPAUSE,
    ansiopen,
    ansiclose,
    ansikopen,
    ansikclose,
    ttgetc,
    ttputc,
    ttflush,
    ansimove,
    ansieeol,
    ansieeop,
    ansibeep,
    ansirev,
    ansicres
#if COLOR
    , ansifcol,
    ansibcol
#endif
};

#if COLOR
/* Optimized foreground color switch */
void ansifcol(int color) 
{
    if (color == cfcolor) return;
    ttputc(ESC);
    ttputc('[');
    ansiparm(color + 30);
    ttputc('m');
    cfcolor = color;
}

/* Optimized background color switch */
void ansibcol(int color) 
{
    if (color == cbcolor) return;
    ttputc(ESC);
    ttputc('[');
    ansiparm(color + 40);
    ttputc('m');
    cbcolor = color;
}
#endif

/* Move cursor to specific row/column (1-based for ANSI) */
int ansimove(int row, int col) 
{
    ttputc(ESC);
    ttputc('[');
    ansiparm(row + 1);
    ttputc(';');
    ansiparm(col + 1);
    ttputc('H');
    return TRUE;
}

/* Erase to End of Line */
void ansieeol(void) 
{
    ttputc(ESC);
    ttputc('[');
    ttputc('K');
}

/* Erase to End of Page */
void ansieeop(void) 
{
#if COLOR
    ansifcol(gfcolor);
    ansibcol(gbcolor);
#endif
    ttputc(ESC);
    ttputc('[');
    ttputc('J');
}

/* * Change reverse video state.
 * Optimized to reset color cache when exiting reverse mode.
 */
void ansirev(int state) 
{
    ttputc(ESC);
    ttputc('[');
    ttputc(state ? '7' : '0');
    ttputc('m');
    
#if COLOR
    if (!state) {
        /* Force color re-sync after SGR 0 */
        int f = cfcolor;
        int b = cbcolor;
        cfcolor = -1;
        cbcolor = -1;
        ansifcol(f);
        ansibcol(b);
    }
#endif
}

int ansicres(void) 
{
    return TRUE;
}

void ansibeep(void) 
{
    ttputc(BEL);
    ttflush();
}

/* * Optimized parameter printer.
 * Avoids recursion and reduces unnecessary operations.
 */
void ansiparm(int n) 
{
    char buf[12];
    int i = 0;
    
    if (n == 0) {
        ttputc('0');
        return;
    }
    
    /* Manual int-to-ascii to avoid sprintf overhead */
    while (n > 0 && i < 10) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) {
        ttputc(buf[--i]);
    }
}

void ansiopen(void) 
{
    char *cp = getenv("TERM");
    
    /* * Modernized check: we accept more than just 'vt100'
     * since almost all modern TERM emulators support ANSI.
     */
    if (cp == NULL) {
        fprintf(stderr, "TERM variable not defined. Defaulting to ANSI.\n");
    }

    strcpy(sres, "NORMAL");
    revexist = TRUE;
    ttopen();
}

void ansiclose(void) 
{
#if COLOR
    /* Reset to light gray on black before exiting */
    ansifcol(7);
    ansibcol(0);
#endif
    ttclose();
}

void ansikopen(void) { /* Keyboard open - noop */ }
void ansikclose(void) { /* Keyboard close - noop */ }

#endif