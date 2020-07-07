#include "fl.hpp"

fl::print_map& fl::instance()
{
    static print_map pm;
    return pm;
}

thread_local worker* g_current_worker=nullptr;

bool fl::in_worker(){ return g_current_worker ? true : false; }
fl::worker& fl::current_worker(){ return *g_current_worker; }

bool fl::in_threadpool()
{ 
    return in_worker() && current_worker().in_threadpool() ? true : false;
}

fl::threadpool& fl::current_threadpool()
{ 
    return current_worker().threadpool();
}

void fl::worker_context::set_current_worker(worker* w)
{ 
    g_current_worker = w; 
}

thread_local threadpool g_default_threadpool;

fl::threadpool& fl::default_threadpool()
{ 
    if(!g_default_threadpool){ g_default_threadpool.start(); }
    return g_default_threadpool;
}

atom fl::schedule(atom a)
{
    if(in_threadpool()){ return current_threadpool().schedule(a); }
    else if(in_worker()){ return current_worker().schedule(a); }
    else{ return default_threadpool().schedule(a); }
}
