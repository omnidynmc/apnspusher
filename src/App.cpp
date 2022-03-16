#include "config.h"

#include <string>

#include <stdarg.h>
#include <stdio.h>

#include <signal.h>
#include <pthread.h>

#include <openframe/openframe.h>

#include "App.h"
#include "Worker.h"

#include "apnspusher.h"

namespace apnspusher {
  using namespace openframe::loglevel;

  const char *App::kPidFile		= "apnspusher.pid";

  App::App(const std::string &prompt, const std::string &config, const bool console)
      : super(prompt, config, console) {
  } // App::App

  App::~App() {
  } // App:~App

  void App::onInitializeSystem() { }

  void App::onInitializeConfig() { }
  void App::onInitializeCommands() { }
  void App::onInitializeDatabase() { }
  void App::onInitializeModules() { }
  void App::onInitializeThreads() {
    std::string source = app->cfg->get_string("app.stompstats.source", "apnspusher");
    std::string instance = app->cfg->get_string("app.stompstats.instance", "devel");
    time_t update_interval = app->cfg->get_int("app.stompstats.interval", 300);
    std::string hosts = app->cfg->get_string("app.stompstats.hosts", "localhost:61613");
    std::string login = app->cfg->get_string("app.stompstats.login", "apnspusher-stompstats");
    std::string passcode = app->cfg->get_string("app.stompstats.passcode", "apnspusher-stompstats");
    int maxqueue = app->cfg->get_int("app.stompstats.maxqueue", 100);
    std::string dest = app->cfg->get_string("app.stompstats.destination", "/topic/stats.devel");

    _stats = new stomp::StompStats(source,
                                   instance,
                                   update_interval,
                                   maxqueue,
                                   hosts,
                                   login,
                                   passcode,
                                   dest);

    _stats->set_elogger(elogger(), elog_name());
    _stats->start();

    int num_workers = cfg->get_int("app.threads.worker", 0);
    for(int i=0; i < num_workers; i++) {
      openframe::ThreadMessage *tm = new openframe::ThreadMessage(i+1);
      tm->var->push_void("app", app);
      tm->var->push_int("id", i+1);
      pthread_t thread_id;
      pthread_create(&thread_id, NULL, App::WorkerThread, tm);
      LOG(LogNotice, << "App: WorkerThread " << thread_id << " Initialized" << std::endl);
      _workers.push_back(thread_id);
    } // for
  } // App::onInitializeThreads

  void App::onDeinitializeSystem() { }
  void App::onDeinitializeCommands() { }
  void App::onDeinitializeDatabase() { }
  void App::onDeinitializeModules() { }
  void App::onDeinitializeThreads() {
    while(!_workers.empty()) {
      pthread_t thread_id = _workers.front();
      LOG(LogNotice, << "App: Waiting for WorkerThread " << thread_id << " to Deinitialize" << std::endl);
      pthread_join(thread_id, NULL);
      _workers.pop_front();
    } // while

    _stats->stop();
    delete _stats;
  } // App::onDeinitializeThreads

  bool App::onRun() {
    return false;
  } // App::onRun

  void App::rcvSighup() {
    LOG(LogNotice, << "### SIGHUP Received" << std::endl);
    elogger()->hup();
  } // App::rcvSighup
  void App::rcvSigusr1() {
    LOG(LogNotice, << "### SIGHUS1 Received" << std::endl);
  } // App::Sigusr1
  void App::rcvSigusr2() {
    LOG(LogNotice, << "### SIGUSR2 Received" << std::endl);
  } // App::Sigusr2
  void App::rcvSigint() {
    LOG(LogNotice, << "### SIGINT Received" << std::endl);
    set_done(true);
  } // App::rcvSigint
  void App::rcvSigpipe() {
    LOG(LogNotice, << "### SIGPIPE Received" << std::endl);
  } // App::rcvSigpipe

  void *App::WorkerThread(void *arg) {
    openframe::ThreadMessage *tm = static_cast<openframe::ThreadMessage *>(arg);
    App *a = static_cast<App *>( tm->var->get_void("app") );
    thread_id_t thread_id = tm->var->get_int("id");

    Worker *worker = new Worker(thread_id,
                                a->cfg->get_string("app.threads.worker.stomp.hosts", "localhost:61613"),
                                a->cfg->get_string("app.threads.worker.stomp.login"),
                                a->cfg->get_string("app.threads.worker.stomp.passcode"),
                                a->cfg->get_string("app.threads.worker.memcached.host", "localhost"),
                                a->cfg->get_string("app.threads.worker.sql.host", "localhost"),
                                a->cfg->get_string("app.threads.worker.sql.user"),
                                a->cfg->get_string("app.threads.worker.sql.pass"),
                                a->cfg->get_string("app.threads.worker.sql.database")
                               );

    worker->set_elogger( a->elogger(), a->elog_name() );
    worker->replace_stats(a->stats(), "apnspusher.worker" + openframe::stringify<int>(thread_id) );

    worker->set_console( a->is_console() );

    worker->init();

    while( !a->is_done() ) {
      bool did_work = worker->run();
      if (!did_work) sleep(2);
    } // while

    delete worker;
    delete tm;

    return NULL;
  } // App::WorkerThread
} // namespace apnspusher
