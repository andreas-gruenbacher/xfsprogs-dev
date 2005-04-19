/*
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <paths.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <xfs/path.h>
#include <xfs/input.h>
#include <xfs/project.h>

#define HAVE_MNTENT_H	1	/* TODO - configure me (+HAVE_GETMNTINFO) */

int fs_count;
struct fs_path *fs_table;
struct fs_path *fs_path;
char *mtab_file;

struct fs_path *
fs_table_lookup(
	const char	*dir,
	uint		flags)
{
	struct stat64	sbuf;
	uint		i;

	if (stat64(dir, &sbuf) < 0)
		return NULL;
	for (i = 0; i < fs_count; i++) {
		if ((flags & fs_table[i].fs_flags) == 0)
			continue;
		if (sbuf.st_dev == fs_table[i].fs_datadev)
			return &fs_table[i];
	}
	return NULL;
}

static int
fs_table_insert(
	char		*dir,
	uint		prid,
	uint		flags,
	char		*fsname,
	char		*fslog,
	char		*fsrt)
{
	struct stat64	sbuf;
	dev_t		datadev, logdev, rtdev;

	if (!dir || !fsname)
		return EINVAL;

	datadev = logdev = rtdev = 0;
	if (stat64(dir, &sbuf) < 0)
		return errno;
	datadev = sbuf.st_dev;
	if (fslog) {
		if (stat64(fslog, &sbuf) < 0)
			return errno;
		logdev = sbuf.st_dev;
	}
	if (fsrt) {
		if (stat64(fsrt, &sbuf) < 0)
			return errno;
		rtdev = sbuf.st_dev;
	}

	fs_table = realloc(fs_table, sizeof(fs_path_t) * (fs_count + 1));
	if (!fs_table)
		goto error;

	fs_path = &fs_table[fs_count];
	fs_path->fs_dir = dir;
	fs_path->fs_prid = prid;
	fs_path->fs_flags = flags;
	fs_path->fs_name = fsname;
	fs_path->fs_log = fslog;
	fs_path->fs_rt = fsrt;
	fs_path->fs_datadev = datadev;
	fs_path->fs_logdev = logdev;
	fs_path->fs_rtdev = rtdev;
	fs_count++;
	return 0;

  error:
	if (dir) free(dir);
	if (fsrt) free(fsrt);
	if (fslog) free(fslog);
	if (fsname) free(fsname);
	return errno;
}

void
fs_table_destroy(void)
{
	while (--fs_count >= 0) {
		free(fs_table[fs_count].fs_name);
		if (fs_table[fs_count].fs_log)
			free(fs_table[fs_count].fs_log);
		if (fs_table[fs_count].fs_rt)
			free(fs_table[fs_count].fs_rt);
		free(fs_table[fs_count].fs_dir);
	}
	if (fs_table)
		free(fs_table);
	fs_table = NULL;
	fs_count = 0;
}


#if defined(HAVE_MNTENT_H)
#include <mntent.h>

static void
fs_extract_mount_options(
	struct mntent	*mnt,
	char		**logp,
	char		**rtp)
{
	char		*fslog, *fsrt, *fslogend, *fsrtend;

	fslog = fsrt = fslogend = fsrtend = NULL;

	/* Extract log device and realtime device from mount options */
	if ((fslog = hasmntopt(mnt, "logdev="))) {
		fslog += 7;
		fslogend = strtok(fslog, " ,");
	}
	if ((fsrt = hasmntopt(mnt, "rtdev="))) {
		fsrt += 6;
		fsrtend = strtok(fsrt, " ,");
	}

	/* Do this only after we've finished processing mount options */
	if (fslog) {
		if (fslogend != fslog)
			*fslogend = '\0'; /* terminate end of logdev name */
		fslog = strdup(fslog);
	}
	if (fsrt) {
		if (fsrtend != fsrt)
			*fsrtend = '\0';  /* terminate end of rtdev name */
		fsrt = strdup(fsrt);
	}

	*logp = fslog;
	*rtp = fsrt;
}

static int
fs_table_initialise_mounts(
	char		*path)
{
	struct mntent	*mnt;
	FILE		*mtp;
	char		*dir = NULL, *fsname = NULL, *fslog, *fsrt;
	int		error = 0, found = 0;

	if (!mtab_file)
		mtab_file = MOUNTED;

	if ((mtp = setmntent(mtab_file, "r")) == NULL)
		return ENOENT;

	while ((mnt = getmntent(mtp)) != NULL) {
		if (strcmp(mnt->mnt_type, "xfs") != 0)
			continue;
		if (path &&
		    ((strcmp(path, mnt->mnt_dir) != 0) &&
		     (strcmp(path, mnt->mnt_fsname) != 0)))
			continue;
		found = 1;
		dir = strdup(mnt->mnt_dir);
		fsname = strdup(mnt->mnt_fsname);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		fs_extract_mount_options(mnt, &fslog, &fsrt);
		if ((error = fs_table_insert(dir, 0, FS_MOUNT_POINT,
						fsname, fslog, fsrt)))
			break;
	}
	endmntent(mtp);
	if (!error && path && !found)
		error = ENXIO;
	if (error) {
		free(dir);
		free(fsname);
	}
	return error;
}

#elif defined(HAVE_GETMNTINFO)
#include <sys/mount.h>

static int
fs_table_initialise_mounts(
	char		*path)
{
	struct statfs	*stats;
	char		*dir = NULL, *fsname = NULL, *fslog = NULL, *fsrt = NULL;
	int		i, count, found = 0, error = 0;

	if ((count = getmntinfo(&stats, 0)) < 0) {
		perror("getmntinfo");
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (strcmp(stats[i].f_fstypename, "xfs") != 0)
			continue;
		if (path &&
		    ((strcmp(path, stats[i].f_mntonname) != 0) &&
		     (strcmp(path, stats[i].f_mntfromname) != 0)))
			continue;
		found = 1;
		dir = strdup(stats[i].f_mntonname);
		fsname = strdup(stats[i].f_mntfromname);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		/* TODO: external log and realtime device? */
		if ((error = fs_table_insert(dir, 0, FS_MOUNT_POINT,
						fsname, fslog, fsrt);
			break;
	}
	if (!error && path && !found)
		error = ENXIO;
	if (error) {
		free(dir);
		free(fsname);
	}
	return error;
}

#else
# error "How do I extract info about mounted filesystems on this platform?"
#endif

/*
 * Given a directory, match it up to a filesystem mount point.
 */
static struct fs_path *
fs_mount_point_from_path(
	const char	*dir)
{
	fs_cursor_t	cursor;
	fs_path_t	*fs;
	struct stat64	s;

	if (stat64(dir, &s) < 0) {
		perror(dir);
		return NULL;
	}

	fs_cursor_initialise(NULL, FS_MOUNT_POINT, &cursor);
	while ((fs = fs_cursor_next_entry(&cursor))) {
		if (fs->fs_datadev == s.st_dev)
			break;
	}
	return fs;
}

static int
fs_table_initialise_projects(
	char		*project)
{
	fs_project_path_t *path;
	fs_path_t	*fs;
	prid_t		prid = 0;
	char		*dir = NULL, *fsname = NULL;
	int		error = 0, found = 0;

	if (project)
		prid = prid_from_string(project);

	setprpathent();
	while ((path = getprpathent()) != NULL) {
		if (project && prid != path->pp_prid)
			continue;
		if ((fs = fs_mount_point_from_path(path->pp_pathname)) == NULL)
			continue;
		found = 1;
		dir = strdup(path->pp_pathname);
		fsname = strdup(fs->fs_name);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		if ((error = fs_table_insert(dir, path->pp_prid,
					FS_PROJECT_PATH, fsname, NULL, NULL)))
			break;
	}
	endprpathent();

	if (!error && project && !found)
		error = ENOENT;
	if (error) {
		free(dir);
		free(fsname);
	}
	return error;
}

void
fs_table_initialise(void)
{
	int		error;

	error = fs_table_initialise_mounts(NULL);
	if (!error)
		error = fs_table_initialise_projects(NULL);
	if (error) {
		fs_table_destroy();
		fprintf(stderr, _("%s: cannot initialise path table: %s\n"),
			progname, strerror(error));
		exit(1);
	}
}

void
fs_table_insert_mount(
	char		*mount)
{
	int		error;

	error = fs_table_initialise_mounts(mount);
	if (error) {
		fs_table_destroy();
		fprintf(stderr, _("%s: cannot setup path for mount %s: %s\n"),
			progname, mount, strerror(error));
		exit(1);
	}
}

void
fs_table_insert_project(
	char		*project)
{
	int		error;

	if (!fs_count) {
		fprintf(stderr, _("%s: no mount table yet, so no projects\n"),
			progname);
		exit(1);
	}
	error = fs_table_initialise_projects(project);
	if (error) {
		fs_table_destroy();
		fprintf(stderr, _("%s: cannot setup path for project %s: %s\n"),
			progname, project, strerror(error));
		exit(1);
	}
}

/*
 * Table iteration (cursor-based) interfaces
 */

void
fs_cursor_initialise(
	char		*dir,
	uint		flags,
	fs_cursor_t	*cur)
{
	fs_path_t	*path;

	memset(cur, 0, sizeof(*cur));
	if (dir) {
		if ((path = fs_table_lookup(dir, flags)) == NULL)
			return;
		cur->local = *path;
		cur->count = 1;
		cur->table = &cur->local;
	} else {
		cur->count = fs_count;
		cur->table = fs_table;
	}
	cur->flags = flags;
}

struct fs_path *
fs_cursor_next_entry(
	fs_cursor_t	*cur)
{
	fs_path_t	*next = NULL;

	while (cur->index < cur->count) {
		next = &cur->table[cur->index++];
		if (cur->flags & next->fs_flags)
			break;
		next = NULL;
	}
	return next;
}