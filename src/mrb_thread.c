#include <mruby.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/variable.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#endif
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int argc;
  mrb_value* argv;
  struct RProc* proc;
  pthread_t thread;
  mrb_state* mrb;
  mrb_value result;
} mrb_thread_context;

static void
mrb_thread_context_free(mrb_state *mrb, void *p) {
  mrb_thread_context* context = (mrb_thread_context*) p;
  if (p) {
    if (context->mrb) mrb_close(context->mrb);
    if (context->argv) free(context->argv);
    free(p);
  }
}

static const struct mrb_data_type mrb_thread_context_type = {
  "mrb_thread_context", mrb_thread_context_free,
};

pthread_mutex_t mutex;

static void*
mrb_thread_func(void* data) {
  mrb_thread_context* context = (mrb_thread_context*) data;
  pthread_mutex_lock(&mutex);
  context->result = mrb_yield_argv(context->mrb, mrb_obj_value(context->proc), context->argc, context->argv);
  pthread_mutex_unlock(&mutex);
  return NULL;
}

static mrb_value
mrb_thread_init(mrb_state* mrb, mrb_value self) {
  mrb_value proc = mrb_nil_value();
  int argc;
  mrb_value* argv;
  mrb_get_args(mrb, "&*", &proc, &argv, &argc);
  if (!mrb_nil_p(proc)) {
    mrb_thread_context* context = (mrb_thread_context*) malloc(sizeof(mrb_thread_context));
    context->mrb = mrb;
    context->proc = mrb_proc_ptr(proc);
    context->argc = argc;
    context->argv = argv;
    context->result = mrb_nil_value();

    mrb_iv_set(mrb, self, mrb_intern(mrb, "context"), mrb_obj_value(
      Data_Wrap_Struct(mrb, mrb->object_class,
      &mrb_thread_context_type, (void*) context)));

    pthread_mutex_init(&mutex, NULL);
    pthread_create(&context->thread, NULL, &mrb_thread_func, (void*) context);
  }
  return self;
}

static mrb_value
mrb_thread_join(mrb_state* mrb, mrb_value self) {
  mrb_value value_context = mrb_iv_get(mrb, self, mrb_intern(mrb, "context"));
  mrb_thread_context* context = NULL;
  Data_Get_Struct(mrb, value_context, &mrb_thread_context_type, context);
  pthread_join(context->thread, NULL);
  pthread_mutex_destroy(&mutex);
  mrb_close(context->mrb);
  context->mrb = NULL;
  return context->result;
}

void
mrb_mruby_thread_gem_init(mrb_state* mrb) {
  struct RClass* _class_thread = mrb_define_class(mrb, "Thread", mrb->object_class);
  mrb_define_method(mrb, _class_thread, "initialize", mrb_thread_init, ARGS_OPT(1));
  mrb_define_method(mrb, _class_thread, "join", mrb_thread_join, ARGS_NONE());
}

void
mrb_mruby_thread_gem_final(mrb_state* mrb) {
}
