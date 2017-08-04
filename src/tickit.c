#include "tickit.h"

#include <signal.h>
#include <sys/time.h>

typedef struct Timer Timer;

struct Timer {
  Timer *next;

  struct timeval at;
  TickitCallbackFn *fn;
  void *user;
};

struct Tickit {
  int refcount;

  volatile int still_running;

  TickitTerm   *term;
  TickitWindow *rootwin;

  Timer *next_timer;
};

Tickit *tickit_new(void)
{
  Tickit *t = malloc(sizeof(Tickit));
  if(!t)
    return NULL;

  t->refcount = 1;

  t->term = NULL;
  t->rootwin = NULL;

  t->next_timer = NULL;

  return t;
}

static void tickit_destroy(Tickit *t)
{
  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term)
    tickit_term_unref(t->term);

  if(t->next_timer) {
    Timer *this, *next;
    for(this = t->next_timer; this; this = next) {
      next = this->next;
      /* TODO: consider if there should be some sort of destroy invocation? */
      free(this);
    }
  }

  free(t);
}

Tickit *tickit_ref(Tickit *t)
{
  t->refcount++;
  return t;
}

void tickit_unref(Tickit *t)
{
  t->refcount--;
  if(!t->refcount)
    tickit_destroy(t);
}

TickitTerm *tickit_get_term(Tickit *t)
{
  if(!t->term) {
    TickitTerm *tt = tickit_term_open_stdio();
    if(!tt)
      return NULL;

    tickit_term_await_started_msec(tt, 50);

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_DRAG);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_KEYPAD_APP, 1);

    tickit_term_clear(tt);

    t->term = tt;
  }

  return t->term;
}

TickitWindow *tickit_get_rootwin(Tickit *t)
{
  if(!t->rootwin) {
    TickitTerm *tt = tickit_get_term(t);
    if(!tt)
      return NULL;

    t->rootwin = tickit_window_new_root(tt);
  }

  return t->rootwin;
}

// TODO: copy the entire SIGWINCH-like structure from term.c
// For now we only handle atmost-one running Tickit instance

static Tickit *running_tickit;

static void sigint(int sig)
{
  if(running_tickit)
    tickit_stop(running_tickit);
}

void tickit_run(Tickit *t)
{
  t->still_running = 1;

  running_tickit = t;
  signal(SIGINT, sigint);

  while(t->still_running) {
    if(t->rootwin)
      tickit_window_flush(t->rootwin);

    int msec = -1;
    if(t->next_timer) {
      struct timeval now, delay;
      gettimeofday(&now, NULL);

      /* next_timer->at - now ==> delay */
      timersub(&t->next_timer->at, &now, &delay);

      msec = (delay.tv_sec * 1000) + (delay.tv_usec / 1000);
      if(msec < 0)
        msec = 0;
    }

    if(t->term)
      tickit_term_input_wait_msec(t->term, msec);
    /* else: er... handle msec somehow */

    if(t->next_timer) {
      struct timeval now;
      gettimeofday(&now, NULL);

      /* timer queue is stored ordered, so we can just eat a prefix
       * of it
       */

      Timer *tim = t->next_timer;
      while(tim) {
        if(timercmp(&tim->at, &now, >))
          break;

        (*tim->fn)(t, tim->user);

        Timer *next = tim->next;
        free(tim);
        tim = next;
      }

      t->next_timer = tim;
    }
  }

  running_tickit = NULL;
}

void tickit_stop(Tickit *t)
{
  t->still_running = 0;
}

/* static for now until we decide how to expose it */
static int tickit_timer_at(Tickit *t, const struct timeval *at, TickitCallbackFn *fn, void *user)
{
  Timer *tim = malloc(sizeof(Timer));
  if(!tim)
    return -1;

  tim->next = NULL;

  tim->at = *at;
  tim->fn = fn;
  tim->user = user;

  Timer **prevp = &t->next_timer;
  /* Try to insert in-order at matching timestamp */
  while(*prevp && !timercmp(&(*prevp)->at, at, >))
    prevp = &(*prevp)->next;

  tim->next = *prevp;
  *prevp = tim;

  return 0;
}

int tickit_timer_after_tv(Tickit *t, const struct timeval *after, TickitCallbackFn *fn, void *user)
{
  struct timeval at;
  gettimeofday(&at, NULL);

  /* at + after ==> at */
  timeradd(&at, after, &at);

  return tickit_timer_at(t, &at, fn, user);
}

int tickit_timer_after_msec(Tickit *t, int msec, TickitCallbackFn *fn, void *user)
{
  return tickit_timer_after_tv(t, &(struct timeval){
      .tv_sec = msec / 1000,
      .tv_usec = (msec % 1000) * 1000,
    }, fn, user);
}
