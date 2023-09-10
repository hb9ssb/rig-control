/*
 * Copyright (c) 2023 Marc Balmer HB9SSB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Control a transceiver using a driver written in Lua */

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "rigd.h"

extern int luaopen_rig(lua_State *);
int rig_control_running = 0;

void *
rig_control(void *arg)
{
	controller_t controller = *(controller_t *)arg;
	struct termios tty;
	lua_State *L;
	struct stat sb;
	int fd;

	rig_control_running = 1;
	pthread_detach(pthread_self());

	fd = open(controller.device, O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "Can't open CAT device %s: %s",
		    controller.device, strerror(errno));
		fd = 0;
		rig_control_running = 0;
		return NULL;
	}
	if (isatty(fd)) {
		if (tcgetattr(fd, &tty) < 0)
			syslog(LOG_ERR, "Can't get CAT device tty attributes");
		else {
			cfmakeraw(&tty);
			tty.c_cflag |= CLOCAL;
			if (tcsetattr(fd, TCSADRAIN, &tty) < 0)
				syslog(LOG_ERR,
				    "Can't set CAT device tty attributes");
		}
	}

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "cannot initialize Lua state");
		rig_control_running = 0;
		return NULL;
	}
	luaL_openlibs(L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	lua_pushcfunction(L, luaopen_rig);
	lua_setfield(L, -2, "rig");

	if (!stat(PATH_INIT, &sb)) {
		if (luaL_dofile(L, PATH_INIT)) {
			syslog(LOG_ERR, "Lua error: %s", lua_tostring(L, -1));
			rig_control_running = 0;
			lua_close(L);
			return NULL;
		}
	}


	printf("rig_control started\n");

	sleep(10);

	printf("rig_control terminates\n");
	lua_close(L);
	rig_control_running = 0;
	return NULL;
}
