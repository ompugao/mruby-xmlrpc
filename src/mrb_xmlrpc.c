#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <string.h>

#include "mrb_xmlrpc.h"

#define OBJECT_GET(mrb, instance, name) \
  mrb_iv_get(mrb, instance, mrb_intern_lit(mrb, name))

#define OBJECT_SET(mrb, instance, name, value) \
  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, name), value)

#define OBJECT_REMOVE(mrb, instance, name) \
  mrb_iv_remove(mrb, instance, mrb_intern_lit(mrb, name))

static void
xmlrpc_raise_if_fault(mrb_state *mrb,xmlrpc_env * const env)
{
    mrb_value mrb_exception_args[2];
    mrb_value mrb_exception;
    if (env->fault_occurred) {
        mrb_exception_args[0] = mrb_fixnum_value(env->fault_code);
        mrb_exception_args[1] = mrb_str_new_cstr(mrb, env->fault_string);
        //mrb_raisef(mrb, E_RUNTIME_ERROR, "XMLRPC ERROR: %s (%d)",env->fault_string,env->fault_code); 
        mrb_exception = mrb_class_new_instance(mrb, 2, mrb_exception_args, E_XMLRPC_FAULTEXCEPTION);
        mrb_exc_raise(mrb, mrb_exception);
    }
}
// ******** client ********
// xmlrpc_client_context container setup /*{{{*/

struct mrb_xmlrpc_client_context {
    xmlrpc_env *env;
    xmlrpc_client *client;
};

static void
mrb_xmlrpc_client_context_free(mrb_state *mrb, void *ptr)
{
    xmlrpc_env_clean((xmlrpc_env *)((struct mrb_xmlrpc_client_context*)ptr)->env);
    xmlrpc_client_destroy((xmlrpc_client *)((struct mrb_xmlrpc_client_context*)ptr)->client);
    mrb_free(mrb,ptr);
}

static struct mrb_data_type mrb_xmlrpc_client_context_type = {"mrb_xmlrpc_client_context" , mrb_xmlrpc_client_context_free };

static mrb_value
mrb_xmlrpc_client_context_wrap(mrb_state *mrb, struct RClass *xmlrpc_class, struct mrb_xmlrpc_client_context *mxcc)
{
    return mrb_obj_value(Data_Wrap_Struct(mrb, xmlrpc_class, &mrb_xmlrpc_client_context_type, mxcc));
}

static mrb_value
mrb_xmlrpc_client_context_make(mrb_state *mrb,mrb_value self){
    struct mrb_xmlrpc_client_context* mxcc;
    mxcc = (struct mrb_xmlrpc_client_context*) mrb_malloc(mrb, sizeof(struct mrb_xmlrpc_client_context));
    mxcc->env = (xmlrpc_env*) mrb_malloc(mrb,sizeof(xmlrpc_env));

    xmlrpc_env_init(mxcc->env);
    xmlrpc_client_setup_global_const(mxcc->env);
    xmlrpc_client_create(mxcc->env, XMLRPC_CLIENT_NO_FLAGS, MRUBY_XMLRPC_NAME, MRUBY_XMLRPC_VERSION, NULL, 0,
                                             (xmlrpc_client**)&(mxcc->client));
    xmlrpc_raise_if_fault(mrb,mxcc->env);
    return mrb_xmlrpc_client_context_wrap(mrb, mrb_obj_class(mrb,self), mxcc);
}
/*}}}*/

// xmlrpc <-> mrb_xmlrpc converter /*{{{*/
static mrb_value
xmlrpc_value_to_mrb_value(mrb_state* mrb, mrb_value self, xmlrpc_env *env, xmlrpc_value* xmlrpc_val){/*{{{*/
    mrb_value ret;
    switch (xmlrpc_value_type(xmlrpc_val)) {
        case XMLRPC_TYPE_INT:
            {
                int int_val;
                xmlrpc_read_int(env, xmlrpc_val, &int_val);
                ret = mrb_fixnum_value((mrb_int)int_val);
                break;
            }
        case XMLRPC_TYPE_I8:
            {
                long long int8_val;
                xmlrpc_read_i8(env, xmlrpc_val, &int8_val);
                ret = mrb_fixnum_value((mrb_int)int8_val);
                break;
            }
        case XMLRPC_TYPE_DOUBLE:
            {
                double double_val;
                xmlrpc_read_double(env, xmlrpc_val, &double_val);
                ret = mrb_float_value(double_val);
                break;
            }
        case XMLRPC_TYPE_BOOL:
            {
                xmlrpc_bool bool_val;
                xmlrpc_read_bool(env, xmlrpc_val, &bool_val);
                if (bool_val) {
                    ret = mrb_true_value();
                } else {
                    ret = mrb_false_value();
                }
                break;
            }
        case XMLRPC_TYPE_DATETIME:
            {
                time_t time_val;
                unsigned int usec_val;
                xmlrpc_read_datetime_usec(env, xmlrpc_val, &time_val, &usec_val);
                mrb_value time_class = mrb_vm_const_get(mrb, mrb_intern_lit(mrb, "Time"));
                ret = mrb_funcall(mrb, time_class, "at", 2 , mrb_float_value(time_val), mrb_float_value(usec_val));
                break;
            }
        case XMLRPC_TYPE_STRING:
            {
                const char* str_val;
                xmlrpc_read_string(env, xmlrpc_val, &str_val);
                ret = mrb_str_new_cstr(mrb, str_val);
                mrb_free(mrb, (void *)str_val);
                break;
            }
        case XMLRPC_TYPE_BASE64:
            {
                size_t length;
                const unsigned char * bytestring;
                xmlrpc_read_base64(env, xmlrpc_val, &length, &bytestring);
                //struct RClass* base64_module = mrb_class_get(mrb,"Base64");
                //ret = mrb_funcall(mrb, base64_class, "decode", 1, mrb_str_new_cstr(mrb, bytestring));
                ret = mrb_str_new_cstr(mrb, (const char*) bytestring);
                break;
            }
        case XMLRPC_TYPE_NIL:
            ret = mrb_nil_value();
            break;
        case XMLRPC_TYPE_STRUCT:
            {
                ret = mrb_hash_new(mrb);
                mrb_value m_key;
                mrb_value m_value;
                xmlrpc_value *xr_key;
                xmlrpc_value *xr_value;
                int n = xmlrpc_struct_size(env, xmlrpc_val);
                int i;
                for (i = 0; i < n; i++) {
                    int ai = mrb_gc_arena_save(mrb);
                    xmlrpc_struct_get_key_and_value(env, xmlrpc_val, i, &xr_key, &xr_value);
                    m_key = xmlrpc_value_to_mrb_value(mrb, self, env, xr_key);
                    m_value = xmlrpc_value_to_mrb_value(mrb, self, env, xr_value);
                    mrb_hash_set(mrb, ret, m_key, m_value);
                    mrb_gc_arena_restore(mrb, ai);
                }
                xmlrpc_DECREF(xr_key);
                xmlrpc_DECREF(xr_value);
                break;
            }
        case XMLRPC_TYPE_ARRAY:
            {
                xmlrpc_value *xr_elm;
                mrb_value mrb_elm;
                ret = mrb_ary_new(mrb);
                int n = xmlrpc_array_size(env, xmlrpc_val);
                int i;
                for (i = 0; i < n; i++) {
                    int ai = mrb_gc_arena_save(mrb);
                    xmlrpc_array_read_item(env, xmlrpc_val, i, &xr_elm);
                    mrb_elm = xmlrpc_value_to_mrb_value(mrb, self, env, xr_elm);
                    mrb_ary_push(mrb, ret, mrb_elm);
                    mrb_gc_arena_restore(mrb, ai);
                }
                xmlrpc_DECREF(xr_elm);
                break;
            }
        default:
            mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid argument");
            break;
    }
    return ret;
}/*}}}*/

static xmlrpc_value *
mrb_value_to_xmlrpc_value(mrb_state* mrb, mrb_value self, xmlrpc_env *env, mrb_value mrb_val){/*{{{*/
    xmlrpc_value * ret;
    switch (mrb_type(mrb_val)) {
        case MRB_TT_FIXNUM:
            ret =  xmlrpc_int_new(env, mrb_fixnum(mrb_val));
            break;
        case MRB_TT_FLOAT:
            ret = xmlrpc_double_new(env, mrb_float(mrb_val));
            break;
        case MRB_TT_SYMBOL:
            ret = xmlrpc_string_new(env, mrb_sym2name(mrb, mrb_symbol(mrb_val)));
            //ret = xmlrpc_string_new(env, mrb_str_to_cstr(mrb, mrb_funcall(mrb, mrb_val, "to_s", 0, NULL)));
            break;
        case MRB_TT_STRING:
            ret = xmlrpc_string_new(env, mrb_str_to_cstr(mrb, mrb_val));
            break;
        case MRB_TT_ARRAY:
            {
                ret = xmlrpc_array_new(env);
                int len = mrb_ary_len(mrb, mrb_val);
                int i;
                for (i = 0; i < len; i++) {
                    int ai = mrb_gc_arena_save(mrb);
                    xmlrpc_value * xr_item;
                    mrb_value m_item;
                    m_item = mrb_ary_ref(mrb, mrb_val, i);
                    xr_item = mrb_value_to_xmlrpc_value(mrb, self, env, m_item);
                    xmlrpc_array_append_item(env, ret, xr_item);
                    xmlrpc_DECREF(xr_item);
                    mrb_gc_arena_restore(mrb, ai);
                }
                break;
            }
        case MRB_TT_HASH:
            {
                ret = xmlrpc_struct_new(env);
                mrb_value keys = mrb_hash_keys(mrb, mrb_val);
                int n = mrb_ary_len(mrb, keys);
                int i;
                for (i = 0; i < n; i++) {
                    int ai = mrb_gc_arena_save(mrb);
                    mrb_value m_key;
                    mrb_value m_value;
                    m_key = mrb_ary_ref(mrb, keys, i);
                    m_value = mrb_hash_get(mrb, mrb_val, m_key);
                    xmlrpc_struct_set_value(env, ret, mrb_str_to_cstr(mrb, mrb_funcall(mrb, m_key, "to_s", 0, NULL)),
                            mrb_value_to_xmlrpc_value(mrb, self, env, m_value));
                    mrb_gc_arena_restore(mrb, ai);
                }
                break;
            }
        case MRB_TT_FALSE:
            ret = xmlrpc_bool_new(env,0);
            break;
        case MRB_TT_TRUE:
            ret = xmlrpc_bool_new(env,1);
            break;
        default:
            if (!strcmp(mrb_obj_classname(mrb, mrb_val), "Time")) {
                ret = xmlrpc_datetime_new_sec(env,
                        mrb_fixnum(mrb_funcall(mrb, mrb_val, "to_i", 0, NULL)));
                break;
            }
            if (!strcmp(mrb_obj_classname(mrb, mrb_val), "XMLRPC::FaultException")) {
                ret = mrb_value_to_xmlrpc_value(mrb, self, env, mrb_funcall(mrb, mrb_val, "to_h",0,NULL));
                break;
            }
            mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid argument");
            ret = xmlrpc_nil_new(env);
            break;
    }
    return ret;
}/*}}}*/
/*}}}*/

// *object* /*{{{*/
static struct RClass *
mrb_xmlrpc_client_class(mrb_state *mrb, mrb_value self)/*{{{*/
{
    struct RClass* _module_xmlrpc = mrb_module_get(mrb,"XMLRPC");
    struct RClass* _class_xmlrpc_client = mrb_class_ptr(mrb_const_get(mrb,mrb_obj_value(_module_xmlrpc),mrb_intern_lit(mrb,"Client")));
    return _class_xmlrpc_client;
}/*}}}*/

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

    OBJECT_SET(mrb,client,"context",mrb_xmlrpc_client_context_make(mrb,client));
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
    mrb_value result_mrbval;
    int argc;

    xmlrpc_value * root_xml; //Array
    xmlrpc_value * result_xml;

    mrb_get_args(mrb,"S|*",&method,&argv,&argc);

    struct mrb_xmlrpc_client_context *mxcc = (struct mrb_xmlrpc_client_context*)(DATA_PTR(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "context"))));
    xmlrpc_env* env = (xmlrpc_env*)(mxcc->env);
    xmlrpc_client* client = (xmlrpc_client*)(mxcc->client);
    
    root_xml = xmlrpc_array_new(env);
    int i = 0;
    for (i = 0; i < argc; i++) {
        xmlrpc_value *xr_elm;
        xr_elm = mrb_value_to_xmlrpc_value(mrb, self, env, argv[i]);
        xmlrpc_array_append_item(env, root_xml, xr_elm);
        xmlrpc_DECREF(xr_elm);
    }

    // setup hostname *{{{*/
    char protocol[9];
    //use ssl or not
    if (mrb_type(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "use_ssl"))) == MRB_TT_TRUE)
    {
        strcpy(protocol,"https://");
    }else{
        strcpy(protocol,"http://");
    }
    // strip the last '/'
    char* hostname = mrb_str_to_cstr(mrb,mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "host")));
    if ( hostname[strlen(hostname)-1] == '/') {
        hostname[strlen(hostname)-1] = '\0';
    }

    // get port string
    char* port = mrb_str_to_cstr(mrb,mrb_funcall(mrb,mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "port")),"to_s",0,NULL));

    // concat
    char* hostname_full;
    hostname_full = (char*) mrb_malloc(mrb, sizeof(char)*(strlen(protocol) + strlen(hostname) + strlen(port)+3));
    *hostname_full = '\0';
    strcat(hostname_full, protocol);
    strcat(hostname_full, hostname);
    strcat(hostname_full, ":");
    strcat(hostname_full, port);
    /*}}}*/

    xmlrpc_server_info *server_info = xmlrpc_server_info_new(env, hostname_full);
    xmlrpc_client_call2(env,client, server_info, RSTRING_PTR(method),root_xml, &result_xml);
    xmlrpc_raise_if_fault(mrb,env);

    result_mrbval = xmlrpc_value_to_mrb_value(mrb, self, env, result_xml);

    mrb_free(mrb,hostname_full);
    xmlrpc_server_info_free(server_info);
    xmlrpc_DECREF(root_xml);
    //xmlrpc_DECREF(result_xml);
    return result_mrbval;
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

// ******** server ********

static struct RClass *
mrb_xmlrpc_server_class(mrb_state *mrb, mrb_value self) {/*{{{*/
    struct RClass* _module_xmlrpc = mrb_module_get(mrb,"XMLRPC");
    struct RClass* _class_xmlrpc_server = mrb_class_ptr(mrb_const_get(mrb,mrb_obj_value(_module_xmlrpc),mrb_intern_lit(mrb,"Server")));
    return _class_xmlrpc_server;
}/*}}}*/

// xmlrpc_server_context container setup /*{{{*/

struct mrb_xmlrpc_server_context {
    xmlrpc_env *env;
};

static void
mrb_xmlrpc_server_context_free(mrb_state *mrb, void *ptr)
{
    xmlrpc_env_clean((xmlrpc_env *)((struct mrb_xmlrpc_server_context*)ptr)->env);
    mrb_free(mrb,ptr);
}

static struct mrb_data_type mrb_xmlrpc_server_context_type = {"mrb_xmlrpc_server_context" , mrb_xmlrpc_server_context_free };

static mrb_value
mrb_xmlrpc_server_context_wrap(mrb_state *mrb, struct RClass *xmlrpc_class, struct mrb_xmlrpc_server_context *mxcc)
{
    return mrb_obj_value(Data_Wrap_Struct(mrb, xmlrpc_class, &mrb_xmlrpc_server_context_type, mxcc));
}

static mrb_value
mrb_xmlrpc_server_context_make(mrb_state *mrb,mrb_value self){
    struct mrb_xmlrpc_server_context* mxsc;
    mxsc = (struct mrb_xmlrpc_server_context*) mrb_malloc(mrb, sizeof(struct mrb_xmlrpc_server_context));
    mxsc->env = (xmlrpc_env*) mrb_malloc(mrb,sizeof(xmlrpc_env));

    xmlrpc_env_init(mxsc->env);
    xmlrpc_raise_if_fault(mrb,mxsc->env);
    return mrb_xmlrpc_server_context_wrap(mrb, mrb_obj_class(mrb,self), mxsc);
}
/*}}}*/

// new(port=8080, host="127.0.0.1", maxConnections=4, stdlog=$stdout, *a)
static mrb_value
mrb_xmlrpc_server_init(mrb_state *mrb, mrb_value self) {/*{{{*/
    mrb_int port = 8080;
    mrb_value host = mrb_str_new_cstr(mrb, "127.0.0.1");
    mrb_int maxConnections = 4;
    mrb_value stdlog = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$stdout"));
    mrb_value rest_argv;
    mrb_int rest_argc;

    mrb_get_args(mrb,"|iSioa",&port,&host,&maxConnections,&stdlog,&rest_argv,&rest_argc);
    mrb_value server = mrb_class_new_instance(mrb, 0, NULL, mrb_xmlrpc_server_class(mrb, self));
    OBJECT_SET(mrb, server, "port", mrb_fixnum_value(port));
    OBJECT_SET(mrb, server, "host", host);
    OBJECT_SET(mrb, server, "maxConnections", mrb_fixnum_value(maxConnections));
    OBJECT_SET(mrb, server, "stdlog", stdlog);
    OBJECT_SET(mrb, server, "context", mrb_xmlrpc_server_context_make(mrb, self));

    // mrb_funcall(mrb, server, "init_uv", 2, mrb_fixnum_value(port), host);
    return server;
}/*}}}*/

static mrb_value
mrb_xmlrpc_server_add_handler(mrb_state *mrb, mrb_value self) {/*{{{*/
    mrb_value proc = mrb_nil_value();
    mrb_value name;
    /*int argc;*/
    /*mrb_value* argv;*/
    /*mrb_get_args(mrb, "&*", &proc, &argv, &argc);*/
    mrb_get_args(mrb, "S&", &name, &proc);
    mrb_value handlers = OBJECT_GET(mrb, self, "handlers");
    if (mrb_nil_p(handlers)) {
        handlers = mrb_hash_new(mrb);
    }
    mrb_funcall(mrb, handlers, "[]=", 2, name, proc);
    OBJECT_SET(mrb, self, "handlers", handlers);
    return mrb_nil_value();
} /*}}}*/

static mrb_value
mrb_xmlrpc_server_set_default_handler(mrb_state *mrb, mrb_value self) {/*{{{*/
    mrb_value proc = mrb_nil_value();
    mrb_get_args(mrb, "&", &proc);
    OBJECT_SET(mrb, self, "default_handler", proc);
    return mrb_nil_value();
}/*}}}*/

// Getter / Setter /*{{{*/
static mrb_value
mrb_xmlrpc_server_get_host(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"host");
}

static mrb_value
mrb_xmlrpc_server_get_port(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"port");
}

static mrb_value
mrb_xmlrpc_server_get_maxConnections(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"maxConnections");
}

static mrb_value
mrb_xmlrpc_server_get_handlers(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"handlers");
}

static mrb_value
mrb_xmlrpc_server_get_default_handler(mrb_state* mrb, mrb_value self) {
    return OBJECT_GET(mrb,self,"default_handler");
}
/*}}}*/

static mrb_value
mrb_xmlrpc_server_parse_xmlrpc_call(mrb_state *mrb, mrb_value self) {/*{{{*/
    const char* xml_str;
    mrb_int xml_str_len;
    mrb_value mrb_method;
    mrb_value mrb_param;
    mrb_value mrb_ret = mrb_ary_new(mrb);
    const char *xr_method;
    xmlrpc_value *xr_param;
    struct mrb_xmlrpc_server_context *mxsc = (struct mrb_xmlrpc_server_context*)(DATA_PTR(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "context"))));

    mrb_get_args(mrb, "s", &xml_str, &xml_str_len);
    xmlrpc_parse_call(mxsc->env, xml_str, strlen(xml_str), &xr_method, &xr_param);

    mrb_method = mrb_str_new_cstr(mrb, xr_method);
    mrb_param = xmlrpc_value_to_mrb_value(mrb, self, mxsc->env, xr_param);
    mrb_funcall(mrb, mrb_ret, "push", 1, mrb_method);
    mrb_funcall(mrb, mrb_ret, "push", 1, mrb_param);
    return mrb_ret;
}/*}}}*/

static mrb_value
mrb_xmlrpc_server_serialize_xmlrpc_response(mrb_state *mrb, mrb_value self) {/*{{{*/
    struct mrb_xmlrpc_server_context *mxsc = (struct mrb_xmlrpc_server_context*)(DATA_PTR(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "context"))));

    mrb_value mrb_response;
    mrb_value mrb_ret;
    xmlrpc_value * xr_val;
    mrb_get_args(mrb, "o", &mrb_response);
    xr_val = mrb_value_to_xmlrpc_value(mrb, self, mxsc->env, mrb_response);
//    if (!mrb_bool(mrb_funcall(mrb, mrb_response, "==", 1, mrb_ary_new(mrb)))) {
        xmlrpc_mem_block * output = XMLRPC_MEMBLOCK_NEW(char, mxsc->env, 0);
        xmlrpc_serialize_response(mxsc->env, output, xr_val);
        XMLRPC_MEMBLOCK_CONTENTS(char,output)[XMLRPC_MEMBLOCK_SIZE(char,output)] = '\0';
        mrb_ret = mrb_str_new_cstr(mrb,XMLRPC_MEMBLOCK_CONTENTS(char, output));
        XMLRPC_MEMBLOCK_FREE(char, output);
        xmlrpc_DECREF(xr_val);
//    }else{
//        mrb_ret = mrb_nil_value();
//    }
    return mrb_ret;
}/*}}}*/

void
mrb_mruby_xmlrpc_gem_init(mrb_state* mrb) {
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

    struct RClass *class_xmlrpc_server = mrb_define_class_under(mrb,module_xmlrpc,"Server",mrb->object_class);

    mrb_define_class_method(mrb, class_xmlrpc_server, "new", mrb_xmlrpc_server_init, ARGS_ANY());
    //mrb_define_method(mrb, class_xmlrpc_server, "shutdown", mrb_xmlrpc_server_serve, ARGS_NONE());

    mrb_define_method(mrb, class_xmlrpc_server, "host", mrb_xmlrpc_server_get_host, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_server, "port", mrb_xmlrpc_server_get_port, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_server, "maxConnections", mrb_xmlrpc_server_get_maxConnections, ARGS_NONE());

    mrb_define_method(mrb, class_xmlrpc_server, "add_handler", mrb_xmlrpc_server_add_handler, ARGS_REQ(2));
    mrb_define_method(mrb, class_xmlrpc_server, "set_default_handler", mrb_xmlrpc_server_set_default_handler, ARGS_REQ(1));
    mrb_define_method(mrb, class_xmlrpc_server, "default_handler", mrb_xmlrpc_server_get_default_handler, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_server, "handlers", mrb_xmlrpc_server_get_handlers, ARGS_NONE());
    mrb_define_method(mrb, class_xmlrpc_server, "parse_xmlrpc_call", mrb_xmlrpc_server_parse_xmlrpc_call, ARGS_REQ(1));
    mrb_define_method(mrb, class_xmlrpc_server, "serialize_xmlrpc_response", mrb_xmlrpc_server_serialize_xmlrpc_response, ARGS_REQ(1));
}

void
mrb_mruby_xmlrpc_gem_final(mrb_state* mrb) {
}
