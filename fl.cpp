#include "fl.hpp"

fl::print_map& fl::instance()
{
    static print_map pm;
    return pm;
}

thread_local worker_context* g_current_worker=nullptr;

fl::worker* fl::worker_context::get_current_worker(){ return g_current_worker; }
void fl::worker_context::set_current_worker(worker* w){ g_current_worker = w; }

bool in_worker(){ return g_current_worker ? true : false; }
fl::worker current_worker(){ return *g_current_worker; }

bool in_workerpool(){ return in_worker() && current_worker().in_workerpool(); }
fl::workerpool current_workerpool(){ return current_worker().get_workerpool(); }

std::mutex g_default_workerpool_mtx;
workerpool g_default_workerpool;
size_t g_default_workerpool_worker_count=0;

void set_default_workerpool_worker_count(size_t cnt)
{
    std::unique_lock<std::mutex> lk(mtx);
    g_default_workerpool_worker_count = cnt;
}

fl::workerpool default_workerpool()
{ 
    if(!g_default_workerpool)
    { 
        if(g_default_workerpool_worker_count){ g_default_workerpool.start(cnt); }
        else{ g_default_workerpool.start(); }
    }
    return g_default_workerpool;
}

atom fl::schedule(atom a)
{
    if(in_workerpool()){ return current_workerpool().schedule(a); }
    else if(in_worker()){ return current_worker().schedule(a); }
    else{ return default_workerpool().schedule(a); }
}
