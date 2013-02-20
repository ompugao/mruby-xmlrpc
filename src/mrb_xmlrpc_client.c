#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/struct.h>

/*static mrb_value*/
/*mrb_c_method(mrb_state *mrb, mrb_value self)*/
/*{*/
    /*return mrb_fixnum_value(10.0);*/
/*}*/

static mrb_value
_xmlrpc_init(mrb_state *mrb,mrb_value self){
    return mrb_nil_value();
}

void
mrb_mruby_xmlrpc_gem_init(mrb_state* mrb) {
    struct RClass *module_xmlrpc = mrb_define_module(mrb, "XMLRPC");
    struct RClass *class_xmlrpc_client = mrb_define_class_under(mrb,module_xmlrpc,"Client",mrb->object_class);
    mrb_define_class_method(mrb, class_xmlrpc_client, "new", _xmlrpc_init, ARGS_REQ(3));
    //mrb_define_class_method(mrb, class_xmlrpc_client, "c_method", mrb_c_method, ARGS_NONE());
}

void
mrb_mruby_xmlrpc_gem_final(mrb_state* mrb) {
    /* free someone */
}
