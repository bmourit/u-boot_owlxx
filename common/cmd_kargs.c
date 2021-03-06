/*
 * (C) Copyright 2012
 * Actions Semi .Inc
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>

static int do_kargs(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *cmd_add = getenv("bootargs.add");
	char *cmd_remove = getenv("bootargs.remove");

	printf("kernel args change - add(%s) remove(%s)\n", cmd_add, cmd_remove);
	boot_append_remove_args(cmd_add, cmd_remove);

	return 0;
}

U_BOOT_CMD(kargs, CONFIG_SYS_MAXARGS, 0, do_kargs,
		"kargs: change kernel command args",
		"command");
