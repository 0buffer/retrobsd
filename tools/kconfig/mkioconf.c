/*
 * Copyright (c) 1980, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include "y.tab.h"
#include "config.h"

static void
comp_config(fp)
    FILE *fp;
{
    register struct file_list *fl;
    register struct device *dp;

    fprintf(fp, "\n#include \"dev/cdvar.h\"\n");
    fprintf(fp, "\nstruct cddevice cddevice[] = {\n");
    fprintf(fp, "/*\tunit\tileave\tflags\tdk\tdevs\t\t\t\t*/\n");

    fl = comp_list;
    while (fl) {
        if (fl->f_type != COMPDEVICE) {
            fl = fl->f_next;
            continue;
        }
        for (dp = dtab; dp != 0; dp = dp->d_next)
            if (dp->d_type == DEVICE &&
                eq(dp->d_name, fl->f_fn) &&
                dp->d_unit == fl->f_compinfo)
                break;
        if (dp == 0)
            continue;
        fprintf(fp, "\t%d,\t%d,\t%d,\t%d,\t{",
            dp->d_unit, dp->d_pri < 0 ? 0 : dp->d_pri,
            dp->d_flags, 1);
        for (fl = fl->f_next; fl->f_type == COMPSPEC; fl = fl->f_next)
            fprintf(fp, " 0x%x,", (unsigned) fl->f_compdev);
        fprintf(fp, " NODEV },\n");
    }
    fprintf(fp, "\t-1,\t0,\t0,\t0,\t{ 0 },\n};\n");
}

/*
 * build the ioconf.c file
 */
static void
pseudo_ioconf(fp)
    register FILE *fp;
{
    register struct device *dp;

    (void)fprintf(fp, "\n#include <sys/device.h>\n\n");
    for (dp = dtab; dp != NULL; dp = dp->d_next)
        if (dp->d_type == PSEUDO_DEVICE)
            (void)fprintf(fp, "extern void %sattach __P((int));\n",
                dp->d_name);
    /*
     * XXX concatonated disks are pseudo-devices but appear as DEVICEs
     * since they don't adhere to normal pseudo-device conventions
     * (i.e. one entry with total count in d_slave).
     */
    if (seen_cd)
        (void)fprintf(fp, "extern void cdattach __P((int));\n");
    /* XXX temporary for HP300, others */
    (void)fprintf(fp, "\n#include <sys/systm.h> /* XXX */\n");
    (void)fprintf(fp, "#define etherattach (void (*)__P((int)))nullop\n");
    (void)fprintf(fp, "#define iteattach (void (*) __P((int)))nullop\n");
    (void)fprintf(fp, "\nstruct pdevinit pdevinit[] = {\n");
    for (dp = dtab; dp != NULL; dp = dp->d_next)
        if (dp->d_type == PSEUDO_DEVICE)
            (void)fprintf(fp, "\t{ %sattach, %d },\n", dp->d_name,
                dp->d_slave > 0 ? dp->d_slave : 1);
    /*
     * XXX count up cds and put out an entry
     */
    if (seen_cd) {
        struct file_list *fl;
        int cdmax = -1;

        for (fl = comp_list; fl != NULL; fl = fl->f_next)
            if (fl->f_type == COMPDEVICE && fl->f_compinfo > cdmax)
                cdmax = fl->f_compinfo;
        (void)fprintf(fp, "\t{ cdattach, %d },\n", cdmax+1);
    }
    (void)fprintf(fp, "\t{ 0, 0 }\n};\n");
    if (seen_cd)
        comp_config(fp);
}

static char *
wnum(num)
{
    if (num == QUES || num == UNKNOWN)
        return ("?");
    (void) sprintf(errbuf, "%d", num);
    return (errbuf);
}

#if MACHINE_PIC32
void pic32_ioconf()
{
    register struct device *dp, *mp;
    FILE *fp;

    fp = fopen(path("ioconf.c"), "w");
    if (fp == 0) {
        perror(path("ioconf.c"));
        exit(1);
    }
    fprintf(fp, "#include \"sys/types.h\"\n");
    fprintf(fp, "#include \"sys/time.h\"\n");
    fprintf(fp, "#include \"mips/dev/device.h\"\n\n");
    fprintf(fp, "#define C (char *)\n\n");

    /* print controller initialization structures */
    for (dp = dtab; dp != 0; dp = dp->d_next) {
        if (dp->d_type == PSEUDO_DEVICE)
            continue;
        fprintf(fp, "extern struct driver %sdriver;\n", dp->d_name);
    }
    fprintf(fp, "\nstruct conf_ctlr conf_cinit[] = {\n");
    fprintf(fp, "/*\tdriver,\t\tunit,\taddr,\t\tpri,\tflags */\n");
    for (dp = dtab; dp != 0; dp = dp->d_next) {
        if (dp->d_type != CONTROLLER && dp->d_type != MASTER)
            continue;
        if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
            printf("can't specify drive/slave for %s%s\n",
                dp->d_name, wnum(dp->d_unit));
            continue;
        }
        if (dp->d_unit == UNKNOWN || dp->d_unit == QUES)
            dp->d_unit = 0;
        fprintf(fp,
            "\t{ &%sdriver,\t%d,\tC 0x%08x,\t%d,\t0x%x },\n",
            dp->d_name, dp->d_unit, dp->d_addr, dp->d_pri,
            dp->d_flags);
    }
    fprintf(fp, "\t{ 0 }\n};\n");

    /* print devices connected to other controllers */
    fprintf(fp, "\nstruct conf_device conf_dinit[] = {\n");
    fprintf(fp,
       "/*driver,\tcdriver,\tunit,\tctlr,\tdrive,\tslave,\tdk,\tflags*/\n");
    for (dp = dtab; dp != 0; dp = dp->d_next) {
        if (dp->d_type == CONTROLLER || dp->d_type == MASTER ||
            dp->d_type == PSEUDO_DEVICE)
            continue;

        mp = dp->d_conn;
        fprintf(fp, "{ &%sdriver,\t", dp->d_name);
        if (mp) {
            fprintf(fp, "&%sdriver,\t%d,\t%d,\t",
                mp->d_name, dp->d_unit, mp->d_unit);
        } else {
            fprintf(fp, "0,\t\t%d,\t0,\t",
                dp->d_unit);
        }
        fprintf(fp, "%d,\t%d,\t%d,\t0x%x },\n",
            dp->d_drive, dp->d_slave, dp->d_dk, dp->d_flags);
    }
    fprintf(fp, "{ 0 }\n};\n");
    pseudo_ioconf(fp);
    (void) fclose(fp);
}
#endif
