/* REGION.C
 *
 * This file contains routines for manipulating regions (the area between
 * the cursor "." and the mark). 
 *
 * This is a specialized fork improving upon Linus Torvalds' uEmacs.
 * Optimized for faster region scanning and reduced logic duplication.
 */

#include <stdio.h>
#include <ctype.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Helper to validate region and check read-only mode */
static int check_region(struct region *rp, int check_view) {
    int s;
    if (check_view && (curbp->b_mode & MDVIEW))
        return rdonly();
    
    if ((s = getregion(rp)) != TRUE)
        return s;
        
    return TRUE;
}

/*
 * Kill the region (C-W).
 * Calculates bounds, moves dot to start, and deletes the range.
 */
int killregion(int f, int n) {
    struct region region;
    int s;

    if ((s = check_region(&region, TRUE)) != TRUE)
        return s;

    if (!(lastflag & CFKILL)) 
        kdelete(); /* Reset kill buffer if last cmd wasn't a kill */

    thisflag |= CFKILL;
    curwp->w_dotp = region.r_linep;
    curwp->w_doto = region.r_offset;
    
    return ldelete(region.r_size, TRUE);
}

/*
 * Copy region to kill buffer (M-W).
 * Dot remains unmoved. Optimized to use direct line pointers.
 */
int copyregion(int f, int n) {
    struct region region;
    struct line *linep;
    int loffs, s;

    if ((s = check_region(&region, FALSE)) != TRUE)
        return s;

    if (!(lastflag & CFKILL))
        kdelete();

    thisflag |= CFKILL;
    linep = region.r_linep;
    loffs = region.r_offset;

    while (region.r_size--) {
        if (loffs == llength(linep)) {
            if ((s = kinsert('\n')) != TRUE) return s;
            linep = lforw(linep);
            loffs = 0;
        } else {
            if ((s = kinsert(lgetc(linep, loffs))) != TRUE) return s;
            ++loffs;
        }
    }
    mlwrite("(region copied)");
    return TRUE;
}

/* * Internal helper for region transformations (Case folding)
 * Reduces code duplication between upper/lower region functions.
 */
static int caseregion(int upper) {
    struct region region;
    struct line *linep;
    int loffs, s, c;

    if ((s = check_region(&region, TRUE)) != TRUE)
        return s;

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;

    while (region.r_size--) {
        if (loffs == llength(linep)) {
            linep = lforw(linep);
            loffs = 0;
        } else {
            c = lgetc(linep, loffs);
            if (upper && islower(c))
                lputc(linep, loffs, toupper(c));
            else if (!upper && isupper(c))
                lputc(linep, loffs, tolower(c));
            ++loffs;
        }
    }
    return TRUE;
}

int lowerregion(int f, int n) { return caseregion(FALSE); }
int upperregion(int f, int n) { return caseregion(TRUE); }

/*
 * getregion: Figures out the bounds between dot and mark.
 * Scan-outward algorithm optimized for speed when dot and mark are close.
 */
int getregion(struct region *rp) {
    struct line *flp, *blp;
    long fsize, bsize;

    if (curwp->w_markp == NULL) {
        mlwrite("No mark set in this window");
        return FALSE;
    }

    /* Case 1: Dot and Mark are on the same line */
    if (curwp->w_dotp == curwp->w_markp) {
        rp->r_linep = curwp->w_dotp;
        if (curwp->w_doto < curwp->w_marko) {
            rp->r_offset = curwp->w_doto;
            rp->r_size = (long)(curwp->w_marko - curwp->w_doto);
        } else {
            rp->r_offset = curwp->w_marko;
            rp->r_size = (long)(curwp->w_doto - curwp->w_marko);
        }
        return TRUE;
    }

    /* Case 2: Scan forward and backward simultaneously to find mark */
    blp = flp = curwp->w_dotp;
    bsize = (long)curwp->w_doto;
    fsize = (long)(llength(flp) - curwp->w_doto + 1);

    while (flp != curbp->b_linep || lback(blp) != curbp->b_linep) {
        /* Move forward */
        if (flp != curbp->b_linep) {
            flp = lforw(flp);
            if (flp == curwp->w_markp) {
                rp->r_linep = curwp->w_dotp;
                rp->r_offset = curwp->w_doto;
                rp->r_size = fsize + curwp->w_marko;
                return TRUE;
            }
            fsize += llength(flp) + 1;
        }
        /* Move backward */
        if (lback(blp) != curbp->b_linep) {
            blp = lback(blp);
            bsize += llength(blp) + 1;
            if (blp == curwp->w_markp) {
                rp->r_linep = blp;
                rp->r_offset = curwp->w_marko;
                rp->r_size = bsize - curwp->w_marko;
                return TRUE;
            }
        }
    }

    mlwrite("Bug: lost mark");
    return FALSE;
}