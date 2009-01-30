#include <ruby.h>
#include "strophe.h"
#include "strophe/common.h"

VALUE mStropheRuby;
VALUE mErrorTypes;
VALUE mLogging;
VALUE mConnectionEvents;
VALUE cConnection;
VALUE cContext;
VALUE cStreamError;
VALUE cEventLoop;
VALUE client_conn_handler;
VALUE cStanza;

/* release the stanza. Called automatically by the GC */
static void t_xmpp_stanza_release(void *stanza) {
    if(stanza != NULL)
      xmpp_stanza_release(stanza);    
}

/* Initialize the strophe library */
VALUE t_xmpp_initialize(VALUE self) {
    xmpp_initialize();
    return Qnil;
}

/* shutdown the library */
VALUE t_xmpp_shutdown(VALUE self) {
    xmpp_shutdown();
    return Qnil;
}

/* check Strophe version */
VALUE t_xmpp_version_check(VALUE self, VALUE major, VALUE minor) {
    int res = xmpp_version_check(FIX2INT(major), FIX2INT(minor));
    return INT2FIX(res);
}

/* parse the stream one time */
VALUE t_xmpp_run_once(VALUE self, VALUE rb_ctx, VALUE timeout) {
    xmpp_ctx_t *ctx;        
    Data_Get_Struct(rb_ctx,xmpp_ctx_t,ctx);
    xmpp_run_once(ctx, NUM2INT(timeout));
    return Qtrue;        
}

/* parse the stream continuously (by calling xmpp_run_once in a while loop) */
VALUE t_xmpp_run(VALUE self, VALUE rb_ctx) {
    xmpp_ctx_t *ctx;
    Data_Get_Struct(rb_ctx,xmpp_ctx_t,ctx);	
    xmpp_run(ctx);
    return Qtrue;
}

/* Set a flag to indicate to our event loop that it must exit */
VALUE t_xmpp_stop(VALUE self, VALUE rb_ctx) {
    xmpp_ctx_t *ctx;
    Data_Get_Struct(rb_ctx, xmpp_ctx_t, ctx);
    xmpp_stop(ctx);
    return Qtrue;
}

/* free the context object (because it causes segmentation error once in a while) */
static VALUE t_xmpp_ctx_free(VALUE self) {
  xmpp_ctx_t *ctx;
  Data_Get_Struct(self,xmpp_ctx_t,ctx);
  xmpp_ctx_free(ctx);
}

/* Get the status of the control loop
 TODO: Define ruby constants for the loop statuses. Currently we have to know them by heart (0 = NOTSTARTED, 1 = RUNNING, 2 = QUIT) 
*/
static VALUE t_xmpp_get_loop_status(VALUE self) {
	xmpp_ctx_t *ctx;
	Data_Get_Struct(self, xmpp_ctx_t, ctx);
	return INT2FIX(ctx->loop_status);
}

/* Set the loop status. Don't call this method if you want to exit the control loop. Call xmpp_stop instead. This method
will set the loop status at QUIT */
static VALUE t_xmpp_set_loop_status(VALUE self, VALUE rb_loop_status) {
	xmpp_ctx_t *ctx;
	Data_Get_Struct(self, xmpp_ctx_t, ctx);
	ctx->loop_status=FIX2INT(rb_loop_status);
	return rb_loop_status;
}

/* Initialize a run time context */
VALUE t_xmpp_ctx_new(VALUE class, VALUE log_level) {
    xmpp_log_t *log;
    xmpp_log_level_t level;
    level=FIX2INT(log_level);
    log = xmpp_get_default_logger((xmpp_log_level_t)level);
    xmpp_ctx_t *ctx = xmpp_ctx_new(NULL, log);
    VALUE tdata = Data_Wrap_Struct(class, 0, free, ctx);
    VALUE argv[1];
    argv[0] = log_level;
    rb_obj_call_init(tdata,1,argv);
    return tdata;
}

/* Ruby initialize for the context. Hmm... do we really need this? */
static VALUE t_xmpp_ctx_init(VALUE self, VALUE log_level) {
  rb_iv_set(self, "@log_level", log_level);
  return self;
}

/* Release the connection object. (Currently not called at all... because it causes segmentation error once in a while) */
static VALUE t_xmpp_conn_release(VALUE self) {
  xmpp_conn_t *conn;
  Data_Get_Struct(self,xmpp_conn_t,conn);
  xmpp_conn_release(conn);
}

/* Initialize a connection object. We register instance variables that will hold the various callbacks
   You should not manipulate instance variables. Use add_handler instead. 
   Maybe put this into xmpp_conn_new?
 */
static VALUE t_xmpp_conn_init(VALUE self, VALUE ctx) {
  rb_iv_set(self, "@ctx", ctx);  
  rb_iv_set(self, "@presence_handlers", rb_ary_new());
  rb_iv_set(self, "@message_handlers", rb_ary_new());
  rb_iv_set(self, "@iq_handlers", rb_ary_new());
  rb_iv_set(self, "@id_handlers", rb_ary_new());
  rb_gv_set("rb_conn",self);
  return self;
}

/* create a connection object then call the initialize method for it*/
VALUE t_xmpp_conn_new(VALUE class, VALUE rb_ctx) {
  //Get the context in a format that C can understand
  xmpp_ctx_t *ctx;
  Data_Get_Struct(rb_ctx, xmpp_ctx_t, ctx);
  
  xmpp_conn_t *conn = xmpp_conn_new(ctx);
  VALUE tdata = Data_Wrap_Struct(class, 0, free, conn);
  VALUE argv[1];
  argv[0] = rb_ctx;
  
  //call ruby "initialize"
  rb_obj_call_init(tdata, 1, argv);
  return tdata;
}

/* Clone a connection */
static VALUE t_xmpp_conn_clone(VALUE self) {
    xmpp_conn_t *conn = xmpp_conn_clone((xmpp_conn_t *)self);    
    return Data_Wrap_Struct(cConnection, 0, t_xmpp_conn_release, conn);
}


/* Get the jid */
static VALUE t_xmpp_conn_get_jid(VALUE self) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);
    return rb_str_new2(xmpp_conn_get_jid(conn));    
}

/* Set the jid */
static VALUE t_xmpp_conn_set_jid(VALUE self, VALUE jid) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);
    xmpp_conn_set_jid(conn, STR2CSTR(jid));
    return jid;
}

/* get the password */
static VALUE t_xmpp_conn_get_pass(VALUE self) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);
    return rb_str_new2(xmpp_conn_get_pass(conn));
}

/* set the password */
static VALUE t_xmpp_conn_set_pass(VALUE self, VALUE pass) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);
    xmpp_conn_set_pass(conn, STR2CSTR(pass));
    return pass;
}

    
/* Parent handler for the connection... we call yield to invoke the client callback*/
static void _conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, 
		  const int error, xmpp_stream_error_t * const stream_error,
		  void * const userdata) {
    if (status == XMPP_CONN_CONNECT) {
	xmpp_info(conn->ctx, "xmpp", "Connected");
	  	    
	
	//yield code block for connection
	if (RTEST(client_conn_handler))
	    rb_funcall(client_conn_handler, rb_intern("call"), 1, INT2FIX(status));
	    
    } else {    	
	xmpp_info(conn->ctx, "xmpp", "Disconnected");
	xmpp_stop(conn->ctx);
    }    
}

/*this is called in a loop (rb_iterate). We invoke the block passed by the user*/
static VALUE _call_handler(VALUE obj, VALUE stanza) {
    rb_funcall(obj, rb_intern("call"),1, stanza);
    return Qnil;
}

/* Called when a message is received in the stream. From there we invoke all code blocks for stanzas of type 'message'*/
int _message_handler(xmpp_conn_t * const conn,
		 xmpp_stanza_t * const stanza,
		 void * const userdata) {    
    VALUE arr;
    VALUE rb_conn;
    rb_conn = rb_gv_get("rb_conn");
    VALUE rb_stanza = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, stanza);
        
    arr=rb_iv_get(rb_conn,"@message_handlers");
    rb_iterate(rb_each,arr,_call_handler, rb_stanza);
    return 1;
}

/* Called when a presence is received in the stream. From there we invoke all code blocks for stanzas of type 'presence'*/
int _presence_handler(xmpp_conn_t * const conn,
		 xmpp_stanza_t * const stanza,
		 void * const userdata) {
    VALUE arr;
    VALUE rb_conn;
    rb_conn = rb_gv_get("rb_conn");
    VALUE rb_stanza = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, stanza);
        
    arr=rb_iv_get(rb_conn,"@presence_handlers");
    rb_iterate(rb_each,arr,_call_handler, rb_stanza);
    return 1;
}

/* Called when a iq is received in the stream. From there we invoke all code blocks for stanzas of type 'iq'*/
int _iq_handler(xmpp_conn_t * const conn,
		 xmpp_stanza_t * const stanza,
		 void * const userdata) {
    VALUE arr;
    VALUE rb_conn;
    rb_conn = rb_gv_get("rb_conn");
    VALUE rb_stanza = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, stanza);
        
    arr=rb_iv_get(rb_conn,"@iq_handlers");
    rb_iterate(rb_each,arr,_call_handler, rb_stanza);
    return 1;
}

/* Called when a id stanza is received TODO: Test this!*/
int _id_handler(xmpp_conn_t * const conn,
		 xmpp_stanza_t * const stanza,
		 void * const userdata) {
    VALUE arr;
    VALUE rb_conn;
    rb_conn = rb_gv_get("rb_conn");
    VALUE rb_stanza = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, stanza);
        
    arr=rb_iv_get(rb_conn,"@id_handlers");
    rb_iterate(rb_each,arr,_call_handler, rb_stanza);
    return 1;
}


/* Add an handler for events in the stream (message, presence or iqs), We store the block we just received in the correct instance variable
 to invoke it later*/
static VALUE t_xmpp_handler_add(VALUE self,VALUE rb_name) {    
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);    
    char *name = STR2CSTR(rb_name);
    VALUE arr;
    xmpp_handler handler;
    
    if(strcmp(name,"message") == 0) {
	arr = rb_iv_get(self, "@message_handlers");	
	handler = _message_handler;
    } else {
	if(strcmp(name,"presence") == 0) {
	    arr = rb_iv_get(self, "@presence_handlers");
	    handler = _presence_handler;
	} else {
	    arr = rb_iv_get(self, "@iq_handlers");
	    handler = _iq_handler;
	}
    }

    xmpp_handler_add(conn, handler, NULL, name, NULL, conn->ctx);
    rb_ary_push(arr, rb_block_proc());        
    return Qnil;
}

/* Add an handler for ID stanzas. TODO:Test this!*/
static VALUE t_xmpp_id_handler_add(VALUE self, VALUE rb_id) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);    
    char *id = STR2CSTR(rb_id);
    VALUE arr;
    
    arr = rb_iv_get(self, "@id_handlers");
    rb_ary_push(arr, rb_block_proc());
    xmpp_id_handler_add(conn, _id_handler, STR2CSTR(id), conn->ctx);
    return Qnil;
}

/* Connect and authenticate. We store the block in the client_conn_handler variable to invoke it later*/
static VALUE t_xmpp_connect_client(VALUE self) {
    xmpp_conn_t *conn;
    xmpp_ctx_t *ctx;
    Data_Get_Struct(rb_iv_get(self, "@ctx"), xmpp_ctx_t, ctx);    
    Data_Get_Struct(self, xmpp_conn_t, conn);
    
    /*The user might have passed a block... however we don't want to invoke it right now.
    We store it to invoke it later in _xmpp_conn_handler */
    if (rb_block_given_p())
	client_conn_handler = rb_block_proc();
    
    int result = xmpp_connect_client(conn, NULL, 0, _conn_handler, ctx);
    return INT2FIX(result);
}

/* Disconnect from the stream. Is it needed? Not too sure about it. Normally if you just call xmpp_stop you should be fine*/
static VALUE t_xmpp_disconnect(VALUE self) {
    xmpp_conn_t *conn;
    Data_Get_Struct(self, xmpp_conn_t, conn);
    xmpp_disconnect(conn);
    return Qtrue;
}

/* Send a stanza in the stream */
static VALUE t_xmpp_send(VALUE self, VALUE rb_stanza) {

    xmpp_conn_t *conn;
    xmpp_stanza_t *stanza;
    
    Data_Get_Struct(self, xmpp_conn_t, conn);
    Data_Get_Struct(rb_stanza, xmpp_stanza_t, stanza);
    
    xmpp_send(conn,stanza);
    return Qtrue;
}
    
/* Create a new stanza */
VALUE t_xmpp_stanza_new(VALUE class) {
  //Get the context in a format that C can understand
  xmpp_ctx_t *ctx;
  VALUE rb_conn = rb_gv_get("rb_conn");
  Data_Get_Struct(rb_iv_get(rb_conn,"@ctx"), xmpp_ctx_t, ctx);
  
  xmpp_stanza_t *stanza = xmpp_stanza_new(ctx);
  VALUE tdata = Data_Wrap_Struct(class, 0, t_xmpp_stanza_release, stanza);
}

/*Clone a stanza. TODO: Test this!*/
static VALUE t_xmpp_stanza_clone(VALUE self) {
    xmpp_stanza_t *stanza;
    xmpp_stanza_t *new_stanza;
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    new_stanza = xmpp_stanza_clone(stanza);
    VALUE tdata = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, new_stanza);
    return tdata;
}

/*Copy a stanza. TODO: Test this!*/
static VALUE t_xmpp_stanza_copy(VALUE self) {
    xmpp_stanza_t *stanza;
    xmpp_stanza_t *new_stanza;
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    new_stanza = xmpp_stanza_copy(stanza);
    VALUE tdata = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, new_stanza);
    return tdata;
}


/*Get the children of the stanza. For example, message stanzas have a "body" child that contains another stanza that contains the actual
message. If you have the top level stanza you can do something like : body_stanza = message_stanza.children */
static VALUE t_xmpp_stanza_get_children(VALUE self) {
    xmpp_stanza_t *stanza;
    xmpp_stanza_t *children;
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    children = xmpp_stanza_get_children(stanza);

    if(children) {
	VALUE tdata = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, children);
        return tdata;
    }
    else
	return Qnil;
}

/*Get the child of a stanza by its name. eg. body_stanza = message_stanza.child_by_name("body")*/
static VALUE t_xmpp_stanza_get_child_by_name(VALUE self, VALUE rb_name) {
    xmpp_stanza_t *stanza;
    xmpp_stanza_t *child;
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *name = STR2CSTR(rb_name);
    child = xmpp_stanza_get_child_by_name(stanza, name);
        
    if(child) {   
	VALUE rb_child = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, child);
        return rb_child;
    } else {
	return Qnil;
    }
}

/*TODO: Test this!*/
static VALUE t_xmpp_stanza_get_next(VALUE self) {
    xmpp_stanza_t *stanza;
    xmpp_stanza_t *next;
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    next = xmpp_stanza_get_next(stanza);

    VALUE tdata = Data_Wrap_Struct(cStanza, 0, t_xmpp_stanza_release, next);
    return tdata;
}

/*Get the attribute of a stanza. eg. message_stanza.child_by_name("body").text*/
static VALUE t_xmpp_stanza_get_attribute(VALUE self, VALUE rb_attribute) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
        
    char *val = xmpp_stanza_get_attribute(stanza,STR2CSTR(rb_attribute));
    return rb_str_new2(val);
}

/*Get the namespace of a stanza TODO: Test this!*/
static VALUE t_xmpp_stanza_get_ns(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *ns = xmpp_stanza_get_ns(stanza);
    return rb_str_new2(ns);
}

/*Get the text of a stanza. */
static VALUE t_xmpp_stanza_get_text(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *text = xmpp_stanza_get_text(stanza);
    
    if(text)
	return rb_str_new2(text);
    else
	return rb_str_new2("");
}

/*Get the name of a stanza (message, presence, iq) */
static VALUE t_xmpp_stanza_get_name(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *name = xmpp_stanza_get_name(stanza);
    return rb_str_new2(name);
}

/*Get the type of a stanza. For example, if the name is 'message', type can be 'chat', 'normal' and so on */
static VALUE t_xmpp_stanza_get_type(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *type = xmpp_stanza_get_type(stanza);
    
    if(type)
	return rb_str_new2(type);
    else
	return Qnil;
}

/*Get the id of a stanza. TODO:Test this!*/
static VALUE t_xmpp_stanza_get_id(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *id = xmpp_stanza_get_id(stanza);
    return rb_str_new2(id);
}

/*Set the value of a stanza attribute (eg. stanza.set_attribute("to","johnsmith@example.com") */
static VALUE t_xmpp_stanza_set_attribute(VALUE self, VALUE rb_attribute, VALUE rb_val) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *attribute = STR2CSTR(rb_attribute);
    char *val = STR2CSTR(rb_val);
    
    xmpp_stanza_set_attribute(stanza, attribute, val);
    return Qtrue;
}

/*Set the namespace of a stanza. TODO:Test this!*/
static VALUE t_xmpp_stanza_set_ns(VALUE self, VALUE rb_ns) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *ns = STR2CSTR(rb_ns);
    
    xmpp_stanza_set_ns(stanza, ns);
    return Qtrue;
}

static VALUE t_xmpp_stanza_to_text(VALUE self) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char* buffer;
    size_t len;
    xmpp_stanza_to_text(stanza, &buffer, &len);
    if (NULL == buffer)
      return rb_str_new(0, 0);
    else
      return rb_str_buf_new2(buffer);
}

/*Set the text of a stanza */
static VALUE t_xmpp_stanza_set_text(VALUE self, VALUE rb_text) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *text = STR2CSTR(rb_text);
    
    xmpp_stanza_set_text(stanza, text);
    return rb_text;    
}

/*Set the name of a stanza (message, presence, iq) */
static VALUE t_xmpp_stanza_set_name(VALUE self, VALUE rb_name) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *name = STR2CSTR(rb_name);
    
    xmpp_stanza_set_name(stanza, name);
    return Qtrue;
}

/*Set the type of a stanza. For example if the name is 'message', the type can be 'chat', 'normal' and so on*/
static VALUE t_xmpp_stanza_set_type(VALUE self, VALUE rb_type) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *type = STR2CSTR(rb_type);
    
    xmpp_stanza_set_type(stanza, type);
    return Qtrue;
}

/*Set the id of a stanza. TODO:Test this!*/
static VALUE t_xmpp_stanza_set_id(VALUE self, VALUE rb_id) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);
    
    char *id = STR2CSTR(rb_id);
    
    xmpp_stanza_set_id(stanza, id);
    return Qtrue;
}

/*Add a child element to a stanza (hint: message stanzas have a body element...)  */
static VALUE t_xmpp_stanza_add_child(VALUE self, VALUE rb_child) {
    xmpp_stanza_t *stanza;    
    Data_Get_Struct(self, xmpp_stanza_t, stanza);

    xmpp_stanza_t *child;    
    Data_Get_Struct(rb_child, xmpp_stanza_t, child);
    int res = xmpp_stanza_add_child(stanza,child);
    return INT2FIX(res);
}


void Init_strophe_ruby() {
    /*Main module that contains everything*/
    mStropheRuby = rb_define_module("StropheRuby");      
        
    /*Wrap the stream_error_t structure into a ruby class named StreamError*/
    cStreamError = rb_define_class_under(mStropheRuby, "StreamError", rb_cObject);
    
    cEventLoop = rb_define_class_under(mStropheRuby, "EventLoop", rb_cObject);
    rb_define_singleton_method(cEventLoop, "run_once", t_xmpp_run_once, 2);
    rb_define_singleton_method(cEventLoop, "prepare", t_xmpp_initialize, 0);
    rb_define_singleton_method(cEventLoop, "run", t_xmpp_run, 1);
    rb_define_singleton_method(cEventLoop, "stop", t_xmpp_stop, 1);
    rb_define_singleton_method(cEventLoop, "shutdown", t_xmpp_shutdown, 0);
    rb_define_singleton_method(cEventLoop, "version", t_xmpp_version_check, 2);
    
    /*Logs*/
    mLogging = rb_define_module_under(mStropheRuby, "Logging");
    rb_define_const(mLogging, "DEBUG", INT2FIX(XMPP_LEVEL_DEBUG));
    rb_define_const(mLogging, "INFO", INT2FIX(XMPP_LEVEL_INFO));
    rb_define_const(mLogging, "WARN", INT2FIX(XMPP_LEVEL_WARN));
    rb_define_const(mLogging, "ERROR", INT2FIX(XMPP_LEVEL_ERROR));

    /*Connection Events*/
    mConnectionEvents = rb_define_module_under(mStropheRuby, "ConnectionEvents");
    rb_define_const(mConnectionEvents, "CONNECT", INT2FIX(XMPP_CONN_CONNECT));
    rb_define_const(mConnectionEvents, "DISCONNECT", INT2FIX(XMPP_CONN_DISCONNECT));
    rb_define_const(mConnectionEvents, "FAIL", INT2FIX(XMPP_CONN_FAIL));
    
    /*Error types*/
    mErrorTypes = rb_define_module_under(mStropheRuby, "ErrorTypes");
    rb_define_const(mErrorTypes, "BAD_FORMAT", INT2FIX(XMPP_SE_BAD_FORMAT));
    rb_define_const(mErrorTypes, "BAD_NS_PREFIX", INT2FIX(XMPP_SE_BAD_NS_PREFIX));
    rb_define_const(mErrorTypes, "CONFLICT", INT2FIX(XMPP_SE_CONFLICT));
    rb_define_const(mErrorTypes, "CONN_TIMEOUT", INT2FIX(XMPP_SE_CONN_TIMEOUT));
    rb_define_const(mErrorTypes, "HOST_GONE", INT2FIX(XMPP_SE_HOST_GONE));
    rb_define_const(mErrorTypes, "HOST_UNKNOWN", INT2FIX(XMPP_SE_HOST_UNKNOWN));
    rb_define_const(mErrorTypes, "IMPROPER_ADDR", INT2FIX(XMPP_SE_IMPROPER_ADDR));
    rb_define_const(mErrorTypes, "INTERNAL_SERVER_ERROR", INT2FIX(XMPP_SE_INTERNAL_SERVER_ERROR));
    rb_define_const(mErrorTypes, "INVALID_FROM", INT2FIX(XMPP_SE_INVALID_FROM));
    rb_define_const(mErrorTypes, "INVALID_ID", INT2FIX(XMPP_SE_INVALID_ID));
    rb_define_const(mErrorTypes, "INVALID_NS", INT2FIX(XMPP_SE_INVALID_NS));
    rb_define_const(mErrorTypes, "INVALID_XML", INT2FIX(XMPP_SE_INVALID_XML));
    rb_define_const(mErrorTypes, "NOT_AUTHORIZED", INT2FIX(XMPP_SE_NOT_AUTHORIZED));
    rb_define_const(mErrorTypes, "POLICY_VIOLATION", INT2FIX(XMPP_SE_POLICY_VIOLATION));
    rb_define_const(mErrorTypes, "REMOTE_CONN_FAILED", INT2FIX(XMPP_SE_REMOTE_CONN_FAILED));
    rb_define_const(mErrorTypes, "RESOURCE_CONSTRAINT", INT2FIX(XMPP_SE_RESOURCE_CONSTRAINT));
    rb_define_const(mErrorTypes, "RESTRICTED_XML", INT2FIX(XMPP_SE_RESTRICTED_XML));
    rb_define_const(mErrorTypes, "SEE_OTHER_HOST", INT2FIX(XMPP_SE_SEE_OTHER_HOST));
    rb_define_const(mErrorTypes, "SYSTEM_SHUTDOWN", INT2FIX(XMPP_SE_SYSTEM_SHUTDOWN));
    rb_define_const(mErrorTypes, "UNDEFINED_CONDITION", INT2FIX(XMPP_SE_UNDEFINED_CONDITION));
    rb_define_const(mErrorTypes, "UNSUPPORTED_ENCODING", INT2FIX(XMPP_SE_UNSUPPORTED_ENCODING));
    rb_define_const(mErrorTypes, "UNSUPPORTED_STANZA_TYPE", INT2FIX(XMPP_SE_UNSUPPORTED_STANZA_TYPE));
    rb_define_const(mErrorTypes, "UNSUPPORTED_VERSION", INT2FIX(XMPP_SE_UNSUPPORTED_VERSION));
    rb_define_const(mErrorTypes, "XML_NOT_WELL_FORMED", INT2FIX(XMPP_SE_XML_NOT_WELL_FORMED));
    
    
    /*Context*/
    cContext = rb_define_class_under(mStropheRuby, "Context", rb_cObject);
    rb_define_singleton_method(cContext, "new", t_xmpp_ctx_new, 1);
    rb_define_method(cContext, "initialize", t_xmpp_ctx_init, 1);    
    rb_define_method(cContext, "free", t_xmpp_ctx_free, 0);
    rb_define_method(cContext, "loop_status", t_xmpp_get_loop_status, 0);
    rb_define_method(cContext, "loop_status=", t_xmpp_set_loop_status, 1);
    
    /*Connection*/
    cConnection = rb_define_class_under(mStropheRuby, "Connection", rb_cObject);
    rb_define_singleton_method(cConnection, "new", t_xmpp_conn_new, 1);
    rb_define_method(cConnection, "initialize", t_xmpp_conn_init, 1);
    rb_define_method(cConnection, "clone", t_xmpp_conn_clone, 1);
    rb_define_method(cConnection, "release", t_xmpp_conn_release, 0);
    rb_define_method(cConnection, "jid", t_xmpp_conn_get_jid,0);
    rb_define_method(cConnection, "jid=", t_xmpp_conn_set_jid,1);
    rb_define_method(cConnection, "password", t_xmpp_conn_get_pass,0);
    rb_define_method(cConnection, "password=", t_xmpp_conn_set_pass,1);
    rb_define_method(cConnection, "connect", t_xmpp_connect_client,0);
    rb_define_method(cConnection, "disconnect", t_xmpp_disconnect, 0);
    rb_define_method(cConnection, "send", t_xmpp_send, 1);

    /*Handlers*/
    rb_define_method(cConnection, "add_handler", t_xmpp_handler_add, 1);
    rb_define_method(cConnection, "add_id_handler", t_xmpp_id_handler_add, 3);

    /*Stanza*/
    cStanza = rb_define_class_under(mStropheRuby, "Stanza", rb_cObject);
    rb_define_singleton_method(cStanza, "new", t_xmpp_stanza_new, 0);
    rb_define_method(cStanza, "clone", t_xmpp_stanza_clone, 0);
    rb_define_method(cStanza, "copy", t_xmpp_stanza_copy, 0);
    //rb_define_method(cStanza, "release", t_xmpp_stanza_release, 0);
    rb_define_method(cStanza, "children", t_xmpp_stanza_get_children, 0);
    rb_define_method(cStanza, "child_by_name", t_xmpp_stanza_get_child_by_name, 1);
    rb_define_method(cStanza, "next", t_xmpp_stanza_get_next, 0);
    rb_define_method(cStanza, "attribute", t_xmpp_stanza_get_attribute, 1);
    rb_define_method(cStanza, "ns", t_xmpp_stanza_get_ns, 0);
    rb_define_method(cStanza, "text", t_xmpp_stanza_get_text, 0);
    rb_define_method(cStanza, "name", t_xmpp_stanza_get_name, 0);
    rb_define_method(cStanza, "add_child", t_xmpp_stanza_add_child, 1);
    rb_define_method(cStanza, "ns=", t_xmpp_stanza_set_ns, 1);
    rb_define_method(cStanza, "to_s", t_xmpp_stanza_to_text, 0);
    rb_define_method(cStanza, "set_attribute", t_xmpp_stanza_set_attribute, 2);    
    rb_define_method(cStanza, "name=", t_xmpp_stanza_set_name, 1);
    rb_define_method(cStanza, "text=", t_xmpp_stanza_set_text, 1);
    rb_define_method(cStanza, "type", t_xmpp_stanza_get_type, 0);
    rb_define_method(cStanza, "id", t_xmpp_stanza_get_id, 0);
    rb_define_method(cStanza, "id=", t_xmpp_stanza_set_id, 1);
    rb_define_method(cStanza, "type=", t_xmpp_stanza_set_type, 1);
}
