#include "callsite.h"

#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>

// Ruby 3 includes native C frames in rb_profile_frames output; those frames
// often have no path (Shopify/rotoscope#87), so a fixed stack offset breaks.
#define RS_CALLSITE_PROFILE_FRAMES 32

static VALUE caller_frame(int *line, bool ruby_call) {
  VALUE frames[RS_CALLSITE_PROFILE_FRAMES];
  int lines[RS_CALLSITE_PROFILE_FRAMES];

  // At this point, for ruby calls, the top ruby stack frame is
  // for the method being called, so we want to skip that frame
  // and get the caller location. This is why we use 1 for ruby
  // calls.
  //
  // However, for C call, the top stack frame is for the caller,
  // so we don't want to have to skip over any extra stack frames
  // for C calls.
  int frame_index = ruby_call ? 1 : 0;

  // There is currently a bug in rb_profile_frames that
  // causes the start argument to effectively always
  // act as if it were 0, so we need to also get the top
  // frame. (https://bugs.ruby-lang.org/issues/14607)
  int captured =
      rb_profile_frames(0, RS_CALLSITE_PROFILE_FRAMES, frames, lines);

  if (captured <= 0) {
    *line = rb_sourceline();
    return Qnil;
  }

  if (ruby_call) {
    int idx = frame_index;
    while (idx < captured) {
      if (!NIL_P(frames[idx]) && !NIL_P(rb_profile_frame_path(frames[idx]))) {
        *line = lines[idx];
        return frames[idx];
      }
      idx++;
    }
    *line = rb_sourceline();
    return Qnil;
  }

  int idx = 0;
  while (idx < captured && NIL_P(frames[idx])) {
    idx++;
  }
  if (idx < captured) {
    *line = lines[idx];
    return frames[idx];
  }
  *line = rb_sourceline();
  return Qnil;
}

rs_callsite_t c_callsite(rb_trace_arg_t *trace_arg) {
  int line;
  VALUE frame = caller_frame(&line, false);
  return (rs_callsite_t){
      .filepath = rb_tracearg_path(trace_arg),
      .lineno = FIX2INT(rb_tracearg_lineno(trace_arg)),
      .method_name =
          NIL_P(frame) ? Qnil : rb_profile_frame_method_name(frame),
      .singleton_p =
          NIL_P(frame) ? Qnil : rb_profile_frame_singleton_method_p(frame),
  };
}

rs_callsite_t ruby_callsite() {
  int line;
  VALUE frame = caller_frame(&line, true);

  VALUE filepath = Qnil;
  VALUE method_name = Qnil;
  VALUE singleton_p = Qnil;

  if (!NIL_P(frame)) {
    filepath = rb_profile_frame_path(frame);
    method_name = rb_profile_frame_method_name(frame);
    singleton_p = rb_profile_frame_singleton_method_p(frame);
  }
  if (NIL_P(filepath)) {
    const char *src = rb_sourcefile();
    if (src != NULL) {
      filepath = rb_str_new2(src);
    }
  }

  return (rs_callsite_t){
      .filepath = filepath,
      .lineno = line,
      .method_name = method_name,
      .singleton_p = singleton_p,
  };
}
