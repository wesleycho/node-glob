#include <v8.h>
#include <glob.h>
#include <node.h>

using namespace node;
using namespace v8;


static Handle<String>
GlobError (int er) {
  switch (er) {
    case GLOB_ABORTED: return String::New("GLOB_ABORTED"); break;
    case GLOB_NOMATCH: return String::New("GLOB_NOMATCH"); break;
    case GLOB_NOSPACE: return String::New("GLOB_NOSPACE"); break;
  }
  
  return String::New("undefined glob error");
}


static Handle<Value> Throw (int);
static Handle<Value> Throw (const char*);
static Handle<Value> Throw (Handle<String>);

static Handle<Value>
Throw (int msg) {
  return Throw(GlobError(msg));
}
static Handle<Value>
Throw (const char* msg) {
  return Throw(String::New(msg));
}
static Handle<Value>
Throw (Handle<String> msg) {
  ThrowException(Exception::Error(msg));
}


// int
// glob(const char *restrict pattern, int flags,
//     int (*errfunc)(const char *epath, int errno), glob_t *restrict pglob);
struct glob_request {
  Persistent<Function> cb;
  glob_t g;
  int retval;
  int flags;
  char *pattern;
};
static int EIO_Glob (eio_req *req) {
  glob_request *gr = (glob_request *)req->data;
  gr->retval = glob(gr->pattern, gr->flags, NULL, &(gr->g));
  return 0;
}
static int EIO_GlobAfter (eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  glob_request *gr = (glob_request *)req->data;
  glob_t g = gr->g;
  
  Local<Value> argv[2];
  if (gr->retval != 0) {
    argv[0] = Exception::Error(GlobError(gr->retval));
  } else {
    Local<Array> pathv = Array::New(g.gl_pathc);
    for (int i = 0; i < g.gl_pathc; i ++) {
      pathv->Set(Integer::New(i), String::New(g.gl_pathv[i]));
    }
    argv[0] = Local<Value>::New(Null());
    argv[1] = pathv;
  }
  globfree(&g);
  
  TryCatch try_catch;
  gr->cb->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  gr->cb.Dispose();
  free(gr);
  return 0;
}

// glob(pattern, flags, cb)
static Handle<Value>
Glob (const Arguments& args) {
  HandleScope scope;
  const char *usage = "usage: glob(pattern, flags, cb)";
  
  if (args.Length() != 3) {
    Throw(usage);
  }

  String::Utf8Value pattern(args[0]);
  int flags = args[1]->Int32Value();
  Local<Function> cb = Local<Function>::Cast(args[2]);
  
  glob_request *gr = (glob_request *)malloc(sizeof(glob_request));
  gr->cb = Persistent<Function>::New(cb);
  gr->pattern = *pattern;
  gr->flags = flags;
  
  eio_custom(EIO_Glob, EIO_PRI_DEFAULT, EIO_GlobAfter, gr);
  ev_ref(EV_DEFAULT_UC);
  
  return Undefined();
}

static Handle<Value>
GlobSync (const Arguments& args)
{
  HandleScope scope;
  const char * usage = "usage: globSync(pattern, flags)";
  if (args.Length() != 2) {
    return Throw(usage);
  }
  
  String::Utf8Value pattern(args[0]);
  
  int flags = args[1]->Int32Value();
  
  glob_t g;
  int retval = glob(*pattern, flags, NULL, &g);
  
  if (retval != 0) {
    globfree(&g);
    return Throw(retval);
  }
  
  // create a JS array
  // loop through the g.gl_pathv adding JS strings to the JS array.
  // then return the JS array.
  Handle<Array> pathv = Array::New(g.gl_pathc);
  for (int i = 0; i < g.gl_pathc; i ++) {
    pathv->Set(Integer::New(i), String::New(g.gl_pathv[i]));
  }
  globfree(&g);
  
  return pathv;
}

extern "C" void
init (Handle<Object> target) 
{
  HandleScope scope;
  NODE_SET_METHOD(target, "glob", Glob);
  NODE_SET_METHOD(target, "globSync", GlobSync);
  
  // flags
  NODE_DEFINE_CONSTANT(target, GLOB_APPEND);
  NODE_DEFINE_CONSTANT(target, GLOB_ERR);
  NODE_DEFINE_CONSTANT(target, GLOB_MARK);
  NODE_DEFINE_CONSTANT(target, GLOB_NOCHECK);
  NODE_DEFINE_CONSTANT(target, GLOB_NOESCAPE);
  NODE_DEFINE_CONSTANT(target, GLOB_NOSORT);
  
  // errors
  NODE_DEFINE_CONSTANT(target, GLOB_ABORTED);
  NODE_DEFINE_CONSTANT(target, GLOB_NOMATCH);
  NODE_DEFINE_CONSTANT(target, GLOB_NOSPACE);
}
