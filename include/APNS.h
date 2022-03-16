/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, mySQL APRS Injector                                        **
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
 $Id: DCC.h,v 1.8 2003/09/04 00:22:00 omni Exp $
 **************************************************************************/

#ifndef APNSPUSHER_CLASS_APNS_H
#define APNSPUSHER_CLASS_APNS_H

#include <pthread.h>

#include <openframe/openframe.h>
#include <apns/apns.h>

namespace apnspusher {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class APNS : public openframe::OpenFrame_Abstract {
    public:
      static const char *kDefaultCaPath;
      static const char *kDefaultCert;
      static const char *kDefaultKey;

      static const char *kDefaultPushHost;
      static const int kDefaultPushPort;
      static const time_t kDefaultPushTimeout;

      static const char *kDefaultFeedbackHost;
      static const int kDefaultFeedbackPort;
      static const time_t kDefaultFeedbackInterval;

      APNS(const unsigned int num_threads, const bool enable_feedback);
      virtual ~APNS();
      APNS &start();
      void stop();

      /**********************
       ** Type Definitions **
       **********************/

      typedef openframe::ThreadQueue<apns::ApnsMessage *> messages_t;
      typedef openframe::ThreadQueue<apns::FeedbackMessage *> feedbacks_t;
      typedef std::set<pthread_t> threadSetType;


      APNS &set_cert(const std::string &cert,
                     const std::string &key);

      APNS &set_push(const std::string &host,
                     const int port,
                     const time_t timeout);

      APNS &set_feedback(const std::string &host,
                         const int port,
                         const time_t interval);

      void push(apns::ApnsMessage *);
      static void *SslThread(void *);
      static void *FeedbackThread(void *);

      void set_done() {
        openframe::scoped_lock slock(&_done_l);
        _done = true;
      } // die()

      bool is_done() {
        openframe::scoped_lock slock(&_done_l);
        return _done;
      } // is_done

      openframe::Stopwatch profile;

    protected:
    private:
      unsigned int _num_threads;
      bool _enable_feedback;

      bool _done;
      openframe::OFLock _done_l;

      openframe::ConfController *_cfg;

      threadSetType _sslThreads;			// ssl thread ids

      messages_t _message_q;
      feedbacks_t _feedback_q;
  }; // APNS

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace apnspusher
#endif
