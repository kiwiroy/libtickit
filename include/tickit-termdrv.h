#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_TERMDRV_H__
#define __TICKIT_TERMDRV_H__

/*
 * The contents of this file should be considered entirely experimental, and
 * subject to any change at any time. We make no API or ABI stability
 * guarantees at this time.
 */

#include "tickit.h"
#include <termkey.h>

typedef struct TickitTermDriver TickitTermDriver;

typedef struct {
  void (*destroy)(TickitTermDriver *ttd);
  void (*start)(TickitTermDriver *ttd);
  int  (*started)(TickitTermDriver *ttd);
  void (*stop)(TickitTermDriver *ttd);
  void (*print)(TickitTermDriver *ttd, const char *str);
  int  (*goto_abs)(TickitTermDriver *ttd, int line, int col);
  void (*move_rel)(TickitTermDriver *ttd, int downward, int rightward);
  int  (*scrollrect)(TickitTermDriver *ttd, int top, int left, int lines, int cols, int downward, int rightward);
  void (*erasech)(TickitTermDriver *ttd, int count, int moveend);
  void (*clear)(TickitTermDriver *ttd);
  void (*chpen)(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final);
  int  (*getctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int *value);
  int  (*setctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int value);
  int  (*setctl_str)(TickitTermDriver *ttd, TickitTermCtl ctl, const char *value);
  int  (*gotkey)(TickitTermDriver *ttd, TermKey *tk, const TermKeyKey *key);
} TickitTermDriverVTable;

struct TickitTermDriver {
  TickitTerm *tt;
  TickitTermDriverVTable *vtable;
};

void *tickit_termdrv_get_tmpbuffer(TickitTermDriver *ttd, size_t len);
void tickit_termdrv_write_str(TickitTermDriver *ttd, const char *str, size_t len);
void tickit_termdrv_write_strf(TickitTermDriver *ttd, const char *fmt, ...);
TickitPen *tickit_termdrv_current_pen(TickitTermDriver *ttd);

#endif

#ifdef __cplusplus
}
#endif
