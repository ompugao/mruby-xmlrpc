#ifndef __MRB_XMLRPC_H__
#define __MRB_XMLRPC_H__

#define MRUBY_XMLRPC_NAME "mruby xmlrpc client"
#define MRUBY_XMLRPC_VERSION "0.0.1"
#define E_XMLRPC_FAULTEXCEPTION  (mrb_class_ptr(mrb_const_get(mrb,mrb_obj_value(mrb_class_get(mrb,"XMLRPC")),mrb_intern(mrb,"FaultException"))))

#endif
