/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __FSTYP_H__
#define __FSTYP_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compatibility macros for IRIX fstyp.h, in case anyone needs them.
 */
#define FSTYPSZ		16	/* max size of fs identifier */
/* Opcodes for the sysfs() system call. */
#define GETFSIND	1	/* translate fs identifier to fstype index */
#define GETFSTYP	2	/* translate fstype index to fs identifier */
#define GETNFSTYP	3	/* return the number of fstypes */
extern int sysfs (int, ...);

/*
 * fstype allows the user to determine the filesystem identifier of
 * mounted or unmounted filesystems, using heuristics.
 * The filesystem type is required by mount(2) and sometimes by mount(8)
 * to mount filesystems of different types.
 */
extern char *fstype (const char * __device);

/*
 * ptabtype allows one to determine the type of partition table in
 * use on a given volume, using heuristics.
 */
extern char *pttype (const char *__device);

#ifdef __cplusplus
}
#endif

#endif	/* __FSTYP_H__ */
