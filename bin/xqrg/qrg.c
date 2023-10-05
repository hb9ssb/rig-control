/*
 * Copyright (c) 2010 - 2022 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 * All rights reserved.
 *
 * This source code is the proprietary confidential property of Micro Systems
 * Marc Balmer, and is provided to licensee solely for documentation and
 * educational purposes. Reproduction, publication, or distribution in any form
 * to any party other than the licensee is strictly prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* main window */

#include <err.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <lualib.h>
#include <lauxlib.h>

#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/MainW.h>
#include <Xm/Label.h>

#ifdef __linux__
#include <bsd/bsd.h>
#endif

#include "xqrg.h"

#define LINELEN	20
static char top_line[LINELEN + 1], bottom_line[LINELEN + 1];
static int x, y;

static Widget top, bottom, status;

extern XtAppContext app;
static XtIntervalId scroll_timeout, update_timeout;
static char *scroll_text;

static void qrg_update(void);

#define SCROLL_TIMEOUT	333	/* approx. 1/3 second */

static void
scroll(XtPointer client_data, XtIntervalId *ignored)
{
	XmString label;
	char *p;
	char c;

	if (scroll_timeout == 0)
		return;
	c = *scroll_text;
	for (p = scroll_text; *(p + 1); p++)
		*p = *(p + 1);
	*p = c;
	label = XmStringCreateLocalized(scroll_text);
	XtVaSetValues(top, XmNlabelString, label, NULL);
	XmStringFree(label);
	if (scroll_timeout != 0)
		scroll_timeout = XtAppAddTimeOut(app, SCROLL_TIMEOUT, scroll,
		    NULL);
}

static void
delayed_scroll(XtPointer client_data, XtIntervalId *ignored)
{
	XmString label;

	label = XmStringCreateLocalized(scroll_text);
	XtVaSetValues(top, XmNlabelString, label, NULL);
	XmStringFree(label);
	scroll_timeout = XtAppAddTimeOut(app, SCROLL_TIMEOUT, scroll, NULL);
}

void
scroll_start(const char *text)
{
	size_t sl;

	sl = strlen(text) + 1 + 8;
	scroll_text = malloc(sl);
	if (scroll_text == NULL)
		return;
	strcpy(scroll_text, text);
	strcat(scroll_text, "        ");
	scroll_timeout = XtAppAddTimeOut(app, SCROLL_TIMEOUT, delayed_scroll,
	    NULL);
}

void
scroll_stop(void)
{
	XmString label;

	if (scroll_timeout != 0) {
		XtRemoveTimeOut(scroll_timeout);
		scroll_timeout = 0;
		label = XmStringCreateLocalized("");
		XtVaSetValues(top,
		    XmNlabelString,	label,
		    NULL);
		XmStringFree(label);
		free(scroll_text);
	}
}

void
clear_display(void)
{
	scroll_stop();
	memset(top_line, ' ', LINELEN);
	memset(bottom_line, ' ', LINELEN);
	qrg_update();
}

void
qrg_status(char *s)
{
	XmString label;

	label = XmStringCreateLocalized(s);
	XtVaSetValues(status, XmNlabelString, label, NULL);
	XmStringFree(label);
}

void
qrg_reset(void)
{
	clear_display();
	x = y = 0;
}

void
qrg_clrscr(void)
{
	clear_display();
	x = y = 0;
}

void
qrg_clreol(void)
{
	int n;
	char *p;

	p = y == 0 ? top_line : bottom_line;
	for (n = y; n < 20; n++)
		*(p + n) = ' ';
	qrg_update();
}

void
qrg_position(int pos)
{
	scroll_stop();

	if (pos < 0 || pos > 40)
		return;
	if (pos < 20) {
		y = 0;
		x = pos;
	} else {
		y = 1;
		x = pos - 20;
	}
}

void
qrg_scrollup(void)
{
	scroll_stop();
	memcpy(top_line, bottom_line, sizeof top_line);
	memset(bottom_line, ' ', LINELEN);
	qrg_update();
}

static void
qrg_delayed_update(XtPointer client_data, XtIntervalId *ignored)
{
	XmString label;

	label = XmStringCreateLocalized(top_line);
	XtVaSetValues(top, XmNlabelString, label, NULL);
	XmStringFree(label);

	label = XmStringCreateLocalized(bottom_line);
	XtVaSetValues(bottom, XmNlabelString, label, NULL);
	XmStringFree(label);
}

static void
qrg_update(void)
{
	if (update_timeout != 0)
		XtRemoveTimeOut(update_timeout);
	update_timeout = XtAppAddTimeOut(app, 100, qrg_delayed_update, NULL);
}

void
qrg_addchar(char c)
{
	char *p;
	scroll_stop();

	p = y == 0 ? top_line : bottom_line;
	*(p + x++) = c;
	if (x == 20) {
		y = 1 - y;
		x = 0;
	}
	qrg_update();
}

void
qrg_addstring(char *s)
{
	char *p;
	scroll_stop();

	p = y == 0 ? top_line : bottom_line;
	for (; *s; s++) {
		*(p + x++) = *s;
		if (x == 20) {
			y = 1 - y;
			x = 0;
		}
	}
	qrg_update();
}

Widget
create_qrg(Widget toplevel)
{
	Widget main_w, main_f, frame;
	XmString label;

	main_w = XtVaCreateManagedWidget("main_window",
	    xmMainWindowWidgetClass,	toplevel,
	    NULL);

	/* construct the main information area */
	main_f = XtVaCreateWidget("xqrg",
	    xmFormWidgetClass,		main_w,
	    XmNfractionBase,		10,
	    NULL);
	label = XmStringCreateLocalized("");
	frame = XtVaCreateManagedWidget("top_frame",
	    xmFrameWidgetClass,		main_f,
	    XmNshadowType,		XmSHADOW_ETCHED_IN,
	    XmNtopAttachment,		XmATTACH_FORM,
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_POSITION,
	    XmNbottomPosition,		4,
	    NULL);
	top = XtVaCreateManagedWidget("top_line",
	    xmLabelWidgetClass,		frame,
	    XmNalignment,		XmALIGNMENT_BEGINNING,
	    XmNlabelString,		label,
	    NULL);
	frame = XtVaCreateManagedWidget("bottom_frame",
	    xmFrameWidgetClass,		main_f,
	    XmNshadowType,		XmSHADOW_ETCHED_IN,
	    XmNtopAttachment,		XmATTACH_POSITION,
	    XmNtopPosition,		4,
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_POSITION,
	    XmNbottomPosition,		8,
	    NULL);
	bottom = XtVaCreateManagedWidget("bottom_line",
	    xmLabelWidgetClass,		frame,
	    XmNalignment,		XmALIGNMENT_BEGINNING,
	    XmNlabelString,		label,
	    NULL);

	status = XtVaCreateManagedWidget("status",
	    xmLabelWidgetClass,		main_f,
	    XmNtopAttachment,		XmATTACH_POSITION,
	    XmNtopPosition,		8,
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_FORM,
	    XmNalignment,		XmALIGNMENT_BEGINNING,
	    XmNlabelString,		label,
	    NULL);

	XmStringFree(label);
	XtManageChild(main_f);

	XtVaSetValues(main_w,
	    XmNworkWindow,	main_f,
	    NULL);

	return main_w;
}
