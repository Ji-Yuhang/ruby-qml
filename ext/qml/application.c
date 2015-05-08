#include "application.h"
#include "interface.h"

VALUE rbqml_cApplication = Qnil;

typedef struct {
    qmlbind_application application;
} application_t;

static void application_free(void *p) {
    application_t *data = (application_t*)p;
    qmlbind_application_release(data->application);
    xfree(data);
}

static const rb_data_type_t data_type = {
    "QML::Application",
    { NULL, &application_free }
};

qmlbind_application rbqml_get_application(VALUE self) {
    application_t *data;
    TypedData_Get_Struct(self, application_t, &data_type, data);
    return data->application;
}

static VALUE application_alloc(VALUE klass) {
    application_t *data = ALLOC(application_t);
    data->application = NULL;
    return TypedData_Wrap_Struct(klass, &data_type, data);
}

static VALUE application_initialize(VALUE self, VALUE args) {
    if (rb_thread_main() != rb_thread_current()) {
        rb_raise(rb_eThreadError, "Initializing QML::Application outside the main thread");
    }

    application_t *data;
    TypedData_Get_Struct(self, application_t, &data_type, data);

    if (rb_type(args) != T_ARRAY) {
        rb_raise(rb_eTypeError, "Expected Array");
    }

    int len = RARRAY_LEN(args);
    char **strs = malloc(len * sizeof(char *));

    for (int i = 0; i < len; ++i) {
        VALUE arg = RARRAY_AREF(args, i);
        strs[i] = rb_string_value_cstr(&arg);
    }

    data->application = qmlbind_application_new(len, strs);

    return self;
}

static void call_callback(qmlbind_backref proc_data) {
    VALUE proc = (VALUE)proc_data;
    rb_proc_call(proc, rb_ary_new());
}

/*
 * Runs a block asynchronously within the event loop.
 *
 * QML UI is not thread-safe and can only be accessed from the main thread.
 * Use this method to set results of asynchronous tasks to UI.
 * @example
 *  def on_button_clicked
 *    Thread.new do
 *      result = do_task
 *      QML.next_tick do
 *        set_result_to_ui(result)
 *      end
 *    end
 *  end
 */
static VALUE application_next_tick(int argc, VALUE *argv, VALUE self) {
    VALUE block;
    rb_scan_args(argc, argv, "&", &block);
    qmlbind_next_tick(rbqml_get_interface(), &call_callback, (qmlbind_backref)block);
    return Qnil;
}

/*
 * Starts the event loop of the application.
 * This method never returns until the application quits.
 */
static VALUE application_exec(VALUE self) {
    return INT2NUM(qmlbind_application_exec(rbqml_get_application(self)));
}

/*
 * Processes queued events in the event loop manually.
 * This method is useful when you are combining an external event loop like EventMachine.
 */
static VALUE application_process_events(VALUE application) {
    qmlbind_process_events();
    return Qnil;
}

void rbqml_init_application(void) {
    rbqml_cApplication = rb_define_class_under(rb_path2class("QML"), "Application", rb_cObject);
    rb_define_alloc_func(rbqml_cApplication, &application_alloc);

    rb_define_private_method(rbqml_cApplication, "initialize", &application_initialize, 1);
    rb_define_method(rbqml_cApplication, "next_tick", &application_next_tick, -1);
    rb_define_method(rbqml_cApplication, "exec", &application_exec, 0);
    rb_define_method(rbqml_cApplication, "process_events", &application_process_events, 0);
}
