#ifndef APNSPUSHER_WORKER_H
#define APNSPUSHER_WORKER_H

#include <string>
#include <vector>
#include <list>

#include <openframe/openframe.h>
#include <openstats/openstats.h>
#include <stomp/Stomp.h>

namespace apnspusher {
/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class Store;
  class APNS;
  class Worker_Exception : public openframe::OpenFrame_Exception {
    public:
      Worker_Exception(const std::string message) throw() : openframe::OpenFrame_Exception(message) { };
  }; // class Worker_Exception

  class Worker : public openframe::LogObject,
                 public openstats::StatsClient_Interface {
    public:
      // ### Constants ### //
      static const int kDefaultStompPrefetch;
      static const time_t kDefaultStatsInterval;
      static const time_t kDefaultMemcachedExpire;
      static const char *kDefaultStompDestNotifyMessages;

      // ### Init ### //
      Worker(const thread_id_t thread_id,
             const std::string &stomp_hosts,
             const std::string &stomp_login,
             const std::string &stomp_passcode,
             const std::string &memcached_host,
             const std::string &db_host,
             const std::string &db_user,
             const std::string &db_pass,
             const std::string &db_database);
      virtual ~Worker();
      void init();
      bool run();
      void try_stats();

      // ### Type Definitions ###

      // ### Options ### //
      Worker &set_console(const bool onoff) {
        _console = onoff;
        return *this;
      } // set_console

      Worker &set_no_send(const bool onoff) {
        _no_send = onoff;
        return *this;
      } // set_no_send

      // ### StatsClient Pure Virtuals ### //
      void onDescribeStats();
      void onDestroyStats();

    protected:
      void try_stompstats();

      bool process_message(const std::string &body);

      struct process_message_t {
        std::string source;
        std::string target;
        std::string id;
        std::string ack;
        std::string reply_id;
        std::string path;
        std::string body;
        bool is_ackonly;
        bool is_to_me;
        openframe::Vars *v;
      }; // process_message_t

      bool event_message_to_apns(process_message_t &pm);

    private:
      // constructor variables
      std::string _stomp_hosts;
      std::string _stomp_login;
      std::string _stomp_passcode;
      std::string _memcached_host;
      std::string _db_host;
      std::string _db_user;
      std::string _db_pass;
      std::string _db_database;
      std::string _aprs_dest;

      std::string _stomp_dest_notify_msgs;

      Store *_store;
      stomp::Stomp *_stomp;
      APNS *_apns;

      bool _connected;
      bool _console;
      bool _no_send;

      struct create_timer_t {
        time_t last_try_at;
        time_t try_interval;
      } _create_timer;

      struct aprs_stats_t {
        unsigned int packet;
        unsigned int position;
        unsigned int message;
        unsigned int telemetry;
        unsigned int status;
        unsigned int capabilities;
        unsigned int peet_logging;
        unsigned int weather;
        unsigned int dx;
        unsigned int experimental;
        unsigned int beacon;
        unsigned int unknown;
        unsigned int reject_invparse;
        unsigned int reject_duplicate;
        unsigned int reject_tosoon;
        unsigned int reject_tofast;
      }; // aprs_stats_t

      struct obj_stats_t {
        unsigned int connects;
        unsigned int disconnects;
        unsigned int packets;
        unsigned int frames_in;
        unsigned int frames_out;
        time_t report_interval;
        time_t last_report_at;
        time_t created_at;
      } _stats;
      void init_stats(obj_stats_t &stats, const bool startup = false);

      struct obj_stompstats_t {
        aprs_stats_t aprs_stats;
        time_t report_interval;
        time_t last_report_at;
        time_t created_at;
      } _stompstats;
      void init_stompstats(obj_stompstats_t &stats, const bool startup = false);
  }; // class Worker

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace apnspusher
#endif
