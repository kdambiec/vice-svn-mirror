/*
 * mui.h
 *
 * Written by
 *  Mathias Roslund <vice.emu@amidog.se>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef _UI_MUI_H
#define _UI_MUI_H

#include "vice.h"
#include "resources.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __USE_INLINE__
#include <exec/memory.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/asl.h>
#ifdef AMIGA_MORPHOS
#include <proto/alib.h> /* for DoMethod */
#endif
#ifdef AMIGA_AROS
#define MUI_OBSOLETE
#endif
#include <proto/muimaster.h>
#if defined(AMIGA_M68K) || defined(AMIGA_AROS)
#include <libraries/mui.h>
#endif

#ifdef AMIGA_AROS
#include <libraries/asl.h>
#undef get
#undef set
#define get(obj,attr,store) GetAttr(attr,obj,(ULONG *)store)
#define set(obj,attr,value) SetAttrs(obj,attr,value,TAG_DONE)
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#define CHECK(store, name) \
  Child, GroupObject, \
    MUIA_Group_Columns, 2, \
    Child, store = CheckMark(FALSE), \
    Child, Label1(name), \
  End,

#define CYCLE(store, name, entries) \
  Child, HGroup, \
    Child, TextObject, \
      MUIA_Text_PreParse, "\033r", \
      MUIA_Text_Contents, name, \
      MUIA_Weight, 30, \
      MUIA_InnerLeft, 0, \
      MUIA_InnerRight, 0, \
    End, \
    Child, store = CycleObject, \
        MUIA_Cycle_Entries, entries, \
        MUIA_Cycle_Active, 0, \
    End, \
  End,

#define MUI_TYPE_NONE    (0)
#define MUI_TYPE_RADIO   (1)
#define MUI_TYPE_CHECK   (2)
#define MUI_TYPE_CYCLE   (3)
#define MUI_TYPE_INTEGER (4)
#define MUI_TYPE_FLOAT   (5)
#define MUI_TYPE_TEXT    (6)

typedef struct {
  APTR object;
  const int type;
  const char *resource;
  const char **strings;
  const int *values;
} ui_to_from_t;

#define UI_END \
  { NULL, MUI_TYPE_NONE, NULL, NULL, NULL }

ui_to_from_t *ui_find_resource(ui_to_from_t *data, char *resource);

int mui_show_dialog(APTR gui, char *title, ui_to_from_t *data);

/* FIXME: remove */
void ui_get_from(ui_to_from_t *data);
void ui_get_to(ui_to_from_t *data);

/* new interface */

#define BTN_OK (32)

int mui_init(void);
APTR mui_get_app(void);
APTR mui_make_simple_window(APTR gui, char *title);
APTR mui_make_ok_cancel_window(APTR gui, char *title);
void mui_add_window(APTR window);
void mui_rem_window(APTR window);
int mui_run(void);
void mui_exit(void);

#endif
