/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** HAL9000, Internet Relay Chat Bot                                     **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************
 $Id: APNS.cpp,v 1.12 2003/09/05 22:23:41 omni Exp $
 **************************************************************************/

#include <fstream>
#include <string>
#include <queue>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <list>
#include <map>
#include <new>
#include <iostream>
#include <fstream>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#include <openframe/openframe.h>
#include <apns/apns.h>

#include "App.h"
#include "APNS.h"

namespace apnspusher {
  using namespace openframe::loglevel;

/**************************************************************************
 ** APNS Class                                                           **
 **************************************************************************/

  const char *APNS::kDefaultCaPath			= "Certs";
  const char *APNS::kDefaultCert			= "Certs/apn-prod-cert.pem";
  const char *APNS::kDefaultKey				= "Certs/apn-prod-key.pem";

  const char *APNS::kDefaultPushHost			= "gateway.push.apple.com";
  const int APNS::kDefaultPushPort			= 2195;
  const time_t APNS::kDefaultPushTimeout		= 3600;

  const char *APNS::kDefaultFeedbackHost		= "gateway.push.apple.com";
  const int APNS::kDefaultFeedbackPort			= 2196;
  const time_t APNS::kDefaultFeedbackInterval		= 86400;

  APNS::APNS(const unsigned int num_threads,
             const bool enable_feedback)
       : _num_threads(num_threads),
         _enable_feedback(enable_feedback),
         _done(false) {

    try {
      _cfg = new openframe::ConfController();
    } // try
    catch(std::bad_alloc xa) {
      assert(false);
    } // catch

    set_cert(kDefaultCert,
             kDefaultKey);

    set_push(kDefaultPushHost,
             kDefaultPushPort,
             kDefaultPushTimeout);

    set_feedback(kDefaultFeedbackHost,
                 kDefaultFeedbackPort,
                 kDefaultFeedbackInterval);

    return;
  } // APNS::APNS

  APNS::~APNS() {
    delete _cfg;
    return;
  } // APNS::~APNS

  APNS &APNS::set_cert(const std::string &cert,
                       const std::string &key) {
    _cfg->replace_string("cert", cert);
    _cfg->replace_string("key", key);
    return *this;
  } // APNS::set_cert

  APNS &APNS::set_push(const std::string &host,
                       const int port,
                       const time_t timeout) {
    _cfg->replace_string("push.host", host);
    _cfg->replace_int("push.port", port);
    _cfg->replace_int("push.timeout", timeout);
    return *this;
  } // APNS::set_push

  APNS &APNS::set_feedback(const std::string &host,
                                const int port,
                                const time_t interval) {
    _cfg->replace_string("feedback.host", host);
    _cfg->replace_int("feedback.port", port);
    _cfg->replace_int("feedback.interval", interval);
    return *this;
  } // APNS::set_feedback

  APNS &APNS::start() {
    // initialize OpenSSL library
    SSL_library_init();

    // Load SSL error strings
    SSL_load_error_strings();

    profile.add("app.apns.push", 300);

    pthread_t sslThread_id;

    // create our signal handling thread
    for(unsigned int i=0; i < _num_threads; i++) {
      openframe::ThreadMessage *tm = new openframe::ThreadMessage(i);
      tm->var->push_void("apns", this);
      tm->var->push_void("message_q", &_message_q);

      tm->var->push_string("host", _cfg->get_string("push.host") );
      tm->var->push_int("port", _cfg->get_int("push.port") );
      tm->var->push_string("cert", _cfg->get_string("cert") );
      tm->var->push_string("key", _cfg->get_string("key") );
      tm->var->push_string("path", kDefaultCaPath );
      tm->var->push_int("timeout", _cfg->get_int("push.timeout") );

      pthread_create(&sslThread_id, NULL, APNS::SslThread, tm);
      LOG(LogInfo, << "Push SSL Thread Started #"
                   << i
                   << ", id "
                   << sslThread_id
                   << std::endl);
      _sslThreads.insert(sslThread_id);
    } // for

    if (_enable_feedback) {
      openframe::ThreadMessage *tm = new openframe::ThreadMessage(0);
      tm->var->push_void("apns", this);
      tm->var->push_void("feedback_q", &_feedback_q);

      tm->var->push_string("host", _cfg->get_string("feedback.host") );
      tm->var->push_int("port", _cfg->get_int("feedback.port") );
      tm->var->push_string("cert", _cfg->get_string("cert") );
      tm->var->push_string("key", _cfg->get_string("key") );
      tm->var->push_string("path", kDefaultCaPath);
      tm->var->push_int("interval", _cfg->get_int("feedback.interval") );

      pthread_create(&sslThread_id, NULL, APNS::FeedbackThread, tm);
      LOG(LogInfo, << "Feedback SSL Thread Started id "
                   << sslThread_id
                   << std::endl);
      _sslThreads.insert(sslThread_id);
    } // if

    return *this;
  } // APNS::start

  void APNS::stop() {
    set_done();

    // create our signal handling thread
    //pthread_cancel(_sslThread_tid);
    // because deinitializeSystem will set die(), we just join the other thread
    // and let it break correctly in order to unload.
    while(!_sslThreads.empty()) {
      threadSetType::iterator ptr = _sslThreads.begin();
      pthread_t tid = (*ptr);

      LOG(LogInfo, << "Waiting for thread to shut down "
                   << tid
                   << std::endl);

      pthread_join(tid, NULL);
      _sslThreads.erase(tid);
    } // while

    LOG(LogInfo, << "Threads shut down successfully"
                 << std::endl);

  } // APNS::stop

  void APNS::push(apns::ApnsMessage *aMessage) {
    assert(aMessage != NULL);
    _message_q.enqueue(aMessage);
  } // APNS::push

  void *APNS::SslThread(void *args) {
    openframe::ThreadMessage *tm = static_cast<openframe::ThreadMessage *>(args);
    openframe::VarController *cfg = tm->var;
    APNS *apns = static_cast<APNS *>( tm->var->get_void("apns") );
    messages_t *message_q = static_cast<messages_t *>( tm->var->get_void("message_q") );

    int maxQueue = app->cfg->get_int("app.apns.ssl.maxqueue", 100);
    int logStatsInterval = app->cfg->get_int("app.apns.ssl.stats.interval", apns::PushController::DEFAULT_STATS_INTERVAL);

    apns::PushController *push;		// prod version
    push = new apns::PushController(cfg->get_string("host"),
                                    cfg->get_int("port"),
                                    cfg->get_string("cert"),
                                    cfg->get_string("key"),
                                    cfg->get_string("path"),
                                    cfg->get_int("timeout")
                                   );

    push->elogger( apns->elogger(), apns->elog_name() );
    push->logStatsInterval(logStatsInterval);

    while(true) {
      if ( apns->is_done() ) break;

      pthread_testcancel();

      apns::ApnsMessage *msg;
      while(push->sendQueueSize() < maxQueue && message_q->dequeue(msg)) {
        push->add(msg);
      } // while

      push->run();

      usleep(100000);
    } // while

    delete push;
    delete tm;

    return NULL;
  } // APNS::SslThread

  void *APNS::FeedbackThread(void *args) {
    openframe::ThreadMessage *tm = static_cast<openframe::ThreadMessage *>(args);
    openframe::VarController *cfg = tm->var;
    APNS *apns = static_cast<APNS *>( tm->var->get_void("apns") );
    feedbacks_t *feedback_q = static_cast<feedbacks_t *>( tm->var->get_void("feedback_q") );

    apns::FeedbackController *feedback;
    feedback = new apns::FeedbackController(cfg->get_string("host"),
                                            cfg->get_int("port"),
                                            cfg->get_string("cert"),
                                            cfg->get_string("key"),
                                            cfg->get_string("path"),
                                            cfg->get_int("interval")
                                           );

    feedback->elogger( apns->elogger(), apns->elog_name() );

    while(true) {
      if (apns->is_done()) break;

      pthread_testcancel();

      feedback->run();

      apns::FeedbackController::messageQueueType removeRegisterQueue;
      if (feedback->getQueue(removeRegisterQueue)) {
        while(!removeRegisterQueue.empty()) {
          apns::FeedbackMessage *fbm = *removeRegisterQueue.begin();
          feedback_q->enqueue(fbm);
          removeRegisterQueue.erase(fbm);
        } // while
      } // if

      sleep(10);
    } // while

    delete feedback;
    delete tm;

    return NULL;
  } // APNS::FeedbackThread
} // namespace apns

