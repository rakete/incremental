/* Filename: resolu.c
 * Created by:  
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * PANDA 3D SOFTWARE
 * Copyright (c) 2001, Disney Enterprises, Inc.  All rights reserved
 *
 * All use of this software is subject to the terms of the Panda 3d
 * Software license.  You should have received a copy of this license
 * along with this source code; you will also find a current copy of
 * the license at http://www.panda3d.org/license.txt .
 *
 * To contact the maintainers of this program write to
 * panda3d@yahoogroups.com .
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Read and write image resolutions.
 */

#include <stdio.h>
#include <stdlib.h>

#include "resolu.h"


char  resolu_buf[RESOLU_BUFLEN];        /* resolution line buffer */

int str2resolu(register RESOLU *, char *);
char *resolu2str(char *, register RESOLU *);

void
fputresolu(int ord, int sl, int ns, FILE *fp)           /* put out picture dimensions */
{
        RESOLU  rs;

        if ((rs.orient = ord) & YMAJOR) {
                rs.xr = sl;
                rs.yr = ns;
        } else {
                rs.xr = ns;
                rs.yr = sl;
        }
        fputsresolu(&rs, fp);
}


int
fgetresolu(int *sl, int *ns, FILE *fp)                  /* get picture dimensions */
{
        RESOLU  rs;

        if (!fgetsresolu(&rs, fp))
                return(-1);
        if (rs.orient & YMAJOR) {
                *sl = rs.xr;
                *ns = rs.yr;
        } else {
                *sl = rs.yr;
                *ns = rs.xr;
        }
        return(rs.orient);
}


char *
resolu2str(char *buf, register RESOLU *rp)              /* convert resolution struct to line */
{
        if (rp->orient&YMAJOR)
                sprintf(buf, "%cY %d %cX %d\n",
                                rp->orient&YDECR ? '-' : '+', rp->yr,
                                rp->orient&XDECR ? '-' : '+', rp->xr);
        else
                sprintf(buf, "%cX %d %cY %d\n",
                                rp->orient&XDECR ? '-' : '+', rp->xr,
                                rp->orient&YDECR ? '-' : '+', rp->yr);
        return(buf);
}


int str2resolu(register RESOLU *rp, char *buf)          /* convert resolution line to struct */
{
        register char  *xndx, *yndx;
        register char  *cp;

        if (buf == NULL)
                return(0);
        xndx = yndx = NULL;
        for (cp = buf; *cp; cp++)
                if (*cp == 'X')
                        xndx = cp;
                else if (*cp == 'Y')
                        yndx = cp;
        if (xndx == NULL || yndx == NULL)
                return(0);
        rp->orient = 0;
        if (xndx > yndx) rp->orient |= YMAJOR;
        if (xndx[-1] == '-') rp->orient |= XDECR;
        if (yndx[-1] == '-') rp->orient |= YDECR;
        if ((rp->xr = atoi(xndx+1)) <= 0)
                return(0);
        if ((rp->yr = atoi(yndx+1)) <= 0)
                return(0);
        return(1);
}
