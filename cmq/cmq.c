/*
# Copyright (C) 2006-2025 Jonathan Michaelson
#
# https://github.com/waytotheweb/scripts
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <https://www.gnu.org/licenses>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#define TARGET "/usr/local/directadmin/plugins/cmq/exec/da_cmq.cgi"

/*
 * This binary is installed setuid root, so the environment it inherits is
 * fully attacker controlled. da_cmq.cgi is a perl script running as root and
 * perl honours PERL5OPT, PERL5LIB and friends, which would allow any caller
 * that passes the admin check to load arbitrary code as root. Only the
 * variables da_cmq.cgi actually needs are forwarded, everything else is
 * discarded and PATH is fixed.
 */
static const char *passthrough[] = {
	"SESSION_ID",
	"SESSION_KEY",
	"QUERY_STRING",
	"POST",
	"REQUEST_METHOD",
	NULL
};

static char *safeenv[8];

static void buildenv (void)
{
	int i;
	int n = 0;

	safeenv[n++] = "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin";
	for (i = 0; passthrough[i] != NULL; i++)
	{
		char *value = getenv(passthrough[i]);
		if (value != NULL && n < (int)(sizeof(safeenv) / sizeof(safeenv[0])) - 1)
		{
			size_t len = strlen(passthrough[i]) + strlen(value) + 2;
			char *entry = malloc(len);
			if (entry == NULL) continue;
			snprintf(entry, len, "%s=%s", passthrough[i], value);
			safeenv[n++] = entry;
		}
	}
	safeenv[n] = NULL;
}

int main (void)
{
	FILE *adminFile;
	uid_t ruid;
	char name[100];
	struct passwd *pw;
	int admin = 0;
	char *newargv[2];

	ruid = getuid();
	pw = getpwuid(ruid);
	if (pw == NULL)
	{
		printf("Permission denied [unknown UID:%d]\n", (int)ruid);
		return 1;
	}

	adminFile=fopen ("/usr/local/directadmin/data/admin/admin.list","r");
	if (adminFile!=NULL)
	{
		while(fgets(name,100,adminFile) != NULL)
		{
			int end = strlen(name) - 1;
			if (end >= 0 && name[end] == '\n') name[end] = '\0';
			if (name[0] != '\0' && strcmp(pw->pw_name, name) == 0) admin = 1;
		}
		fclose(adminFile);
	}
	if (admin == 1)
	{
		/* Set the group first: once the uid is dropped to root the gid can
		   still be changed, but failing silently must never be an option */
		if (setgid(0) != 0)
		{
			printf("Failed to set gid\n");
			return 1;
		}
		if (setuid(0) != 0)
		{
			printf("Failed to set uid\n");
			return 1;
		}

		buildenv();

		newargv[0] = TARGET;
		newargv[1] = NULL;

		execve(TARGET, newargv, safeenv);
		printf("Failed to execute %s\n", TARGET);
		return 1;
	} else {
		printf("Permission denied [User:%s UID:%d]\n", pw->pw_name, (int)ruid);
	}
	return 0;
}
