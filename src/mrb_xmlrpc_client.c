#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/data.h>
//#include <mruby/struct.h>
#include <mruby/class.h>
#include <mruby/variable.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <string.h>

#define MRUBY_XMLRPC_NAME "mruby xmlrpc client"
#define MRUBY_XMLRPC_VERSION "0.0.1"

#define OBJECT_GET(mrb, instance, name) \
  mrb_iv_get(mrb, instance, mrb_intern(mrb, name))

#define OBJECT_SET(mrb, instance, name, value) \
  mrb_iv_set(mrb, instance, mrb_intern(mrb, name), value)

#define OBJECT_REMOVE(mrb, instance, name) \
  mrb_iv_remove(mrb, instance, mrb_intern(mrb, name))

static void
xmlrpc_raise_if_fault(mrb_state *mrb,xmlrpc_env * const env)
{
    if (env->fault_occurred) {
        mrb_raisef(mrb, E_RUNTIME_ERROR, "XMLRPC ERROR: %s (%d)",env->fault_string,env->fault_code); \
    }
}

// xmlrpc_env container setup /*{{{*/

static void
mrb_xmlrpc_env_free(mrb_state *mrb, void *ptr)
{
    xmlrpc_env_clean(ptr);
    //mrb_free(mrb,ptr);
}

static struct mrb_data_type mrb_xmlrpc_env_type = {"mrb_xmlrpc_env" , mrb_xmlrpc_env_free };

static mrb_value
mrb_xmlrpc_env_wrap(mrb_state *mrb, struct RClass *xmlrpc_class, xmlrpc_env *mme)
{
    return mrb_obj_value(Data_Wrap_Struct(mrb, xmlrpc_class, &mrb_xmlrpc_env_type, mme));
}

static mrb_value
mrb_xmlrpc_env_make(mrb_state *mrb,mrb_value self){
    xmlrpc_env* env;
    env = (xmlrpc_env*) mrb_malloc(mrb,sizeof(xmlrpc_env));
    xmlrpc_env_init(env);
    xmlrpc_client_init2(env,XMLRPC_CLIENT_NO_FLAGS,MRUBY_XMLRPC_NAME,MRUBY_XMLRPC_VERSION,NULL,0);
    xmlrpc_raise_if_fault(mrb,env);
    return mrb_xmlrpc_env_wrap(mrb, mrb_obj_class(mrb,self), env);
}
/*}}}*/

// xmlrpc <-> mrb_xmlrpc converter /*{{{*/
static void
xmlrpc_value_to_mrb_value(mrb_state* mrb, xmlrpc_value* xmlrpc_val, mrb_value* mrb_val){
}

static void
mrb_value_to_xmlrpc_value(mrb_state* mrb, mrb_value * mrb_val, xmlrpc_value * xmlrpc_val){
}
/*}}}*/

// *object* /*{{{*/
static struct RClass *
mrb_xmlrpc_client_class(mrb_state *mrb, mrb_value self)
{
    struct RClass* _module_xmlrpc = mrb_class_get(mrb,"XMLRPC");
    struct RClass* _class_xmlrpc_client = mrb_class_ptr(mrb_const_get(mrb,mrb_obj_value(_module_xmlrpc),mrb_intern(mrb,"Client")));
    return _class_xmlrpc_client;
}

static mrb_value
mrb_xmlrpc_client_init(mrb_state *mrb, mrb_value self) {/*{{{*/
    mrb_value host;
    mrb_value path;
    mrb_int   port;
    mrb_value proxy_host = mrb_nil_value();
    mrb_int   proxy_port;
    mrb_value user;
    mrb_value password;
    mrb_value use_ssl = mrb_false_value();
    mrb_value time_out;

    mrb_get_args(mrb,"SSi|SiSSbi",&host,&path,&port,&proxy_host,&proxy_port,&user,&password,&use_ssl,&time_out);

    mrb_value client;
    client = mrb_class_new_instance(mrb, 0 /*argc*/, NULL /*argv*/,mrb_xmlrpc_client_class(mrb,self));

    OBJECT_SET(mrb,client,"env",mrb_xmlrpc_env_make(mrb,client));
    OBJECT_SET(mrb,client,"host",host);
    OBJECT_SET(mrb,client,"path",path);
    OBJECT_SET(mrb,client,"port",mrb_fixnum_value(port));
    OBJECT_SET(mrb,client,"proxy_host",proxy_host);
    OBJECT_SET(mrb,client,"proxy_port",mrb_fixnum_value(proxy_port));
    OBJECT_SET(mrb,client,"user",user);
    OBJECT_SET(mrb,client,"password",password);
    OBJECT_SET(mrb,client,"use_ssl",use_ssl);
    OBJECT_SET(mrb,client,"time_out",time_out);

    return client;
}
/*}}}*/

static mrb_value
mrb_xmlrpc_client_call(mrb_state *mrb, mrb_value self ) {/*{{{*/
    mrb_value method;
    mrb_value *argv;
    int argc;

    xmlrpc_value * root_xml; //Array
    xmlrpc_value * result_xml;

    mrb_get_args(mrb,"S|*",&method,&argv,&argc);

    //mrb_value mrbenv = OBJECT_GET(mrb,self,"env");
    xmlrpc_env* env = (xmlrpc_env*)DATA_PTR(mrb_iv_get(mrb, self, mrb_intern(mrb, "env")));
    
    root_xml = xmlrpc_array_new(env);
    int i = 0;
    for (i = 0; i < argc; i++) {
        switch (mrb_type(argv[i])) {
            case MRB_TT_FIXNUM:
                xmlrpc_int_new(env,argv[i].value.i);
                break;
            case MRB_TT_FLOAT:
                break;
            default:
                break;
        }
    }

    // setup hostname *{{{*/
    char protocol[9];
    //use ssl or not
    if (mrb_type(mrb_iv_get(mrb, self, mrb_intern(mrb, "use_ssl"))) == MRB_TT_TRUE)
    {
        strcpy(protocol,"https://");
    }else{
        strcpy(protocol,"http://");
    }
    // strip the last '/'
    char* hostname = mrb_str_to_cstr(mrb,mrb_iv_get(mrb, self, mrb_intern(mrb, "host")));
    if ( hostname[strlen(hostname)-1] == '/') {
        hostname[strlen(hostname)-1] = NULL;
    }
    // get port string
    char* port = mrb_str_to_cstr(mrb,mrb_funcall(mrb,mrb_iv_get(mrb, self, mrb_intern(mrb, "port")),"to_s",0,NULL));

    // concat
    char* hostname_full;
    hostname_full = (char*) mrb_malloc(mrb, sizeof(char)*(strlen(protocol) + strlen(hostname) + strlen(port)+3));
    *hostname_full = NULL;
    strcat(hostname_full, protocol);
    strcat(hostname_full, hostname);
    strcat(hostname_full, ":");
    strcat(hostname_full, port);
    /*}}}*/

    xmlrpc_value * result = xmlrpc_client_call(env,
            hostname_full,
            mrb_str_to_cstr(mrb,method),
            "(ii)",(xmlrpc_int32) 5,(xmlrpc_int32) 3);
    xmlrpc_raise_if_fault(mrb,env);
    xmlrpc_int32 sum;
    xmlrpc_read_int(env,result,&sum);
    xmlrpc_raise_if_fault(mrb,env);
    mrb_warn("add result:%d\n", sum);
//    xmlrpc_client_call2f(//&env,
//            mrb_
//            mrb_iv_get(mrb,)
//            ,RSTRING_PTR(method),root_xml,&result_xml);

    mrb_free(mrb,hostname_full);
    xmlrpc_DECREF(root_xml);
    //xmlrpc_DECREF(result_xml);
    return mrb_nil_value();
}
/*}}}*/

// Getter / Setter/*{{{*/
static mrb_value
mrb_xmlrpc_client_get_host(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"host");
}

static mrb_value
mrb_xmlrpc_client_get_path(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"path");
}

static mrb_value
mrb_xmlrpc_client_get_port(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"port");
}

static mrb_value
mrb_xmlrpc_client_set_host(mrb_state* mrb, mrb_value self) {
    mrb_value arg;
    mrb_get_args(mrb,"S",&arg);
    OBJECT_SET(mrb,self,"host",arg);
    return mrb_nil_value();
}

static mrb_value
mrb_xmlrpc_client_set_port(mrb_state* mrb, mrb_value self) {
    mrb_int arg;
    mrb_get_args(mrb,"i",&arg);
    OBJECT_SET(mrb,self,"port",mrb_fixnum_value(arg));
    return mrb_nil_value();
}

static mrb_value
mrb_xmlrpc_client_set_path(mrb_state* mrb, mrb_value self) {
    mrb_value arg;
    mrb_get_args(mrb,"S",&arg);
    OBJECT_SET(mrb,self,"path",arg);
    return mrb_nil_value();
}/*}}}*/
/*}}}*/

void
mrb_mruby_xmlrpc_client_gem_init(mrb_state* mrb) {
    struct RClass *module_xmlrpc = mrb_define_module(mrb, "XMLRPC");
    struct RClass *class_xmlrpc_client = mrb_define_class_under(mrb,module_xmlrpc,"Client",mrb->object_class);

    int ai = mrb_gc_arena_save(mrb);
    mrb_define_class_method(mrb, class_xmlrpc_client, "new", mrb_xmlrpc_client_init, ARGS_REQ(3) | ARGS_OPT(6));

    mrb_define_method(mrb, class_xmlrpc_client, "host", mrb_xmlrpc_client_get_host, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_client, "host=", mrb_xmlrpc_client_set_host, ARGS_REQ(1));
    mrb_define_method(mrb, class_xmlrpc_client, "path", mrb_xmlrpc_client_get_path, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_client, "path=", mrb_xmlrpc_client_set_path, ARGS_REQ(1));
    mrb_define_method(mrb, class_xmlrpc_client, "port", mrb_xmlrpc_client_get_port, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_client, "port=", mrb_xmlrpc_client_set_port, ARGS_REQ(1));

    mrb_define_method(mrb, class_xmlrpc_client, "call", mrb_xmlrpc_client_call, ARGS_REQ(1) && ARGS_REST());
    mrb_gc_arena_restore(mrb, ai);
}

void
mrb_mruby_xmlrpc_client_gem_final(mrb_state* mrb) {
}
